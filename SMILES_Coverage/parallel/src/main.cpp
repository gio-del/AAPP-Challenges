#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <unordered_set>
#include <vector>
#include <mpi.h>

#include "mpi_error_check.hpp"

#define MAX_LINE_LENGTH 1024

// set the maximum size of the ngram
static constexpr size_t max_pattern_len = 3;
static constexpr size_t max_dictionary_size = 128;
static_assert(max_pattern_len > 1, "The pattern must contain at least one character");
static_assert(max_dictionary_size > 1, "The dictionary must contain at least one element");

// global variables that hold the message tags
const int tag_size = 0; // tag to send the num of lines, such that each process can allocate a recvbuf of the right size

struct mpi_context_type {
  MPI_Comm comm;
  int rank;
  int size;

  mpi_context_type() {
    MPI_Comm_dup(MPI_COMM_WORLD, &comm);
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);
  }

  void update() {
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);
  }
};

// simple class to represent a word of our dictionary
struct word {
  char ngram[max_pattern_len + 1];  // the string data, statically allocated
  size_t size = 0;                  // the string size
  size_t coverage = 0;              // the score of the word
};

// utility struct to ease working with words
struct word_coverage_lt_comparator {
  bool operator()(const word &w1, const word &w2) const { return w1.coverage < w2.coverage; }
};
struct word_coverage_gt_comparator {
  bool operator()(const word &w1, const word &w2) const { return w1.coverage > w2.coverage; }
};

// this is our dictionary of ngram
struct dictionary {
  std::vector<word> data;
  std::vector<word>::iterator worst_element;

  /**
   * add a new word to the dictionary, if the dictionary is full, it will replace the worst element
  */
  void add_word(word &new_word) {
    const auto coverage = new_word.coverage;
    if (data.size() < max_dictionary_size) {
      data.emplace_back(std::move(new_word));
      worst_element = std::end(data);
    } else {
      if (worst_element == std::end(data)) {
        worst_element = std::min_element(std::begin(data), std::end(data), word_coverage_lt_comparator{});
      }
      if (coverage > worst_element->coverage) {
        *worst_element = std::move(new_word);
        worst_element = std::end(data);
      }
    }
  }

  void write(std::ostream &out) const {
    for (const auto &word : data) {
      out << word.ngram << ' ' << word.coverage << std::endl;
    }
    out << std::flush;
  }
};

// TODO: Function handle to be used in MPI_Reduce to create a custom MPI_Op
void reduce_word(word *in, word *inout, int *len, MPI_Datatype *type) {
    fprintf(stderr, "REDUCE WORD\n");
    int max_len = *len; // Maximum length of the arrays

    for (int i = 0; i < max_len; ++i) {
        bool found = false;
        for (int j = 0; j < max_len; ++j) {
            if (strcmp(in[i].ngram, inout[j].ngram) == 0) {
                inout[j].coverage += in[i].coverage;
                found = true;
                break;
            }
        }
        if (!found) {
            inout[*len] = in[i];
            (*len)++;
        }
    }
}

size_t count_coverage(const std::string &dataset, const char *ngram) {
  size_t index = 0;
  size_t counter = 0;
  const size_t ngram_size = ::strlen(ngram);
  while (index < dataset.size()) {
    index = dataset.find(ngram, index);
    if (index != std::string::npos) {
      ++counter;
      index += ngram_size;
    }
  }
  return counter * ngram_size;
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[]) {
  // initialize MPI
  int provided_thread_level;
  int rc_init = MPI_Init_thread(&argc, &argv, MPI_THREAD_SINGLE, &provided_thread_level);
  exit_on_fail(rc_init);
  if(provided_thread_level < MPI_THREAD_SINGLE) {
    std::cerr << "The MPI implementation does not support multiple threads" << std::endl;
    return EXIT_FAILURE;
  }

  // get the MPI context
  mpi_context_type mpi_context = mpi_context_type();

  // create the custom MPI_Op
  MPI_Op mpi_reduce_word_op;
  int rc_create_op = MPI_Op_create((MPI_User_function *) reduce_word, 1, &mpi_reduce_word_op);
  exit_on_fail(rc_create_op);

  // create and commit the MPI_Datatype for the string
  MPI_Datatype mpi_string_type;
  int rc_create_type = MPI_Type_contiguous(MAX_LINE_LENGTH, MPI_CHAR, &mpi_string_type);
  exit_on_fail(rc_create_type);
  int rc_commit_type = MPI_Type_commit(&mpi_string_type);
  exit_on_fail(rc_commit_type);

  // create the MPI_Datatype for the word
  MPI_Datatype mpi_word_type;
  int block_lengths[3] = {max_pattern_len + 1, 1, 1};
  MPI_Aint displacements[3] = {offsetof(word, ngram), offsetof(word, size), offsetof(word, coverage)};
  MPI_Datatype types[3] = {MPI_CHAR, MPI_UINT64_T, MPI_UINT64_T};
  int rc_create_word_type = MPI_Type_create_struct(3, block_lengths, displacements, types, &mpi_word_type);
  exit_on_fail(rc_create_word_type);
  int rc_commit_word_type = MPI_Type_commit(&mpi_word_type);
  exit_on_fail(rc_commit_word_type);

  // Idea: the master process (rank 0) reads the input and distributes uniformly the work
  // among the other processes. Each process will compute the coverage of a certain number of ngrams
  // at the end all processes will reduce the results to the master process that will compute the final dictionary
  // that will be printed to the standard output
  // DO NOT: print anything to anywhere unless already present in the code

  // This is the vector of lines that will be read from the standard input by the master process
  std::vector<std::string> lines;

  // This is the final words vector that will be used to store the reduced words, the master process will convert it to a dictionary and print it
  word *final_words = new word[max_dictionary_size * mpi_context.size];
  int final_words_len;

  // Total number of lines read from the standard input
  int num_lines;

  if(mpi_context.rank == 0) {
    // Read The input from the standard input
    std::cerr << "Reading the molecules from the standard input ..." << std::endl;

    for(std::string line; std::getline(std::cin, line); /* automatically handled */) {
      lines.push_back(std::move(line));
    }

    fprintf(stderr, "Process %d read %ld lines\n", mpi_context.rank, lines.size());

    num_lines = lines.size();

    // Send it to the other processes
    for(int i = 1; i < mpi_context.size; ++i) {
      int rc_send = MPI_Send(&num_lines, 1, MPI_INT, i, tag_size, mpi_context.comm);
      exit_on_fail(rc_send);
    }
  } else {
    int rc_recv = MPI_Recv(&num_lines, 1, MPI_INT, 0, tag_size, mpi_context.comm, MPI_STATUS_IGNORE);
    exit_on_fail(rc_recv);
    fprintf(stderr, "Process %d knows that there are %d lines in total\n", mpi_context.rank, num_lines);
  }

  int rc_barrier = MPI_Barrier(mpi_context.comm);
  exit_on_fail(rc_barrier);

  fprintf(stderr, "Process %d starting scatter\n", mpi_context.rank);

  // Scatter Data From Root to all processes
  MPI_Count *sendcounts = new MPI_Count[mpi_context.size];
  MPI_Count *displs = new MPI_Count[mpi_context.size];
  MPI_Count offset = 0;

  // Send buff is basically lines but with contiguous memory
  char sendbuf[num_lines][MAX_LINE_LENGTH];

  if(mpi_context.rank == 0) {
    // Copy the lines to sendbuf
    for(int i = 0; i < num_lines; ++i) {
      strcpy(sendbuf[i], lines[i].c_str());
    }
  }

  for(int i = 0; i < mpi_context.size; ++i) {
    sendcounts[i] = num_lines / mpi_context.size;
    if(i < num_lines % mpi_context.size) {
      sendcounts[i]++;
    }
    displs[i] = offset;
    offset += sendcounts[i];
  }

  // We have to define the recvbuf
  char recvbuf[sendcounts[mpi_context.rank]][MAX_LINE_LENGTH];

  int rc_scatter = MPI_Scatterv_c(sendbuf, sendcounts, displs, mpi_string_type, recvbuf, sendcounts[mpi_context.rank], mpi_string_type, 0, mpi_context.comm);
  exit_on_fail(rc_scatter);

  fprintf(stderr, "Process %d starting processing\n", mpi_context.rank);
  // Print the recvbuf to check if it is correct
  fprintf(stderr, "Process %d recvbuf: \n", mpi_context.rank);
  for(int i = 0; i < sendcounts[mpi_context.rank]; ++i) {
    fprintf(stderr, "%s\n", recvbuf[i]);
  }

  // Now recvbuf is ready to be used and contains the lines to be processed
  std::unordered_set<char> alphabet_builder;
  std::string database; // reserve 50MB
  database.reserve(50000000);

  for(std::string line: recvbuf) {
    for(const auto character : line) {
      alphabet_builder.emplace(character);
      database.push_back(character);
    }
  }

  // put the alphabet in a container with random access capabilities
  std::vector<char> alphabet;
  alphabet_builder.reserve(alphabet_builder.size());
  std::for_each(std::begin(alphabet_builder), std::end(alphabet_builder),
                [&alphabet](const auto character) { alphabet.push_back(character); });

  // precompute the number of permutations according to the number of characters
  auto permutations = std::vector(max_pattern_len, alphabet.size());
  for (std::size_t i{1}; i < permutations.size(); ++i) {
    permutations[i] = alphabet.size() * permutations[i - std::size_t{1}];
  }

  // declare the dictionary that holds all the ngrams with the greatest coverage of the dictionary
  dictionary result;

  // this outer loop goes through the n-gram with different sizes
  for (std::size_t ngram_size{1}; ngram_size <= max_pattern_len; ++ngram_size) {
    fprintf(stderr, "Process %d computing ngrams of size %zu\n", mpi_context.rank, ngram_size);
    // this loop goes through all the permutation of the current ngram-size
    const auto num_words = permutations[ngram_size - std::size_t{1}];
    for (std::size_t word_index{0}; word_index < num_words; ++word_index) {
      // compose the ngram
      word current_word;
      memset(current_word.ngram, '\0', max_pattern_len + 1);
      for (std::size_t character_index{0}, remaining_size = word_index; character_index < ngram_size;
          ++character_index, remaining_size /= alphabet.size()) {
        current_word.ngram[character_index] = alphabet[remaining_size % alphabet.size()];
      }
      current_word.size = ngram_size;

      // evaluate the coverage and add the word to the dictionary
      current_word.coverage = count_coverage(database, current_word.ngram);
      result.add_word(current_word);
    }
  }

  int rc_reduce_barrier = MPI_Barrier(mpi_context.comm);
  exit_on_fail(rc_reduce_barrier);
  // Now each process has a dictionary with the ngrams with the greatest coverage, we need to reduce them to the master process
  // summing the coverage of the same ngrams and then the master process will compute the final dictionary

  // Remove the processors that have no words from the communicator
  if(result.data.size() > 0) {
    fprintf(stderr, "Process %d ready to reduce\n", mpi_context.rank);
  } else {
    fprintf(stderr, "Process %d has no words\n", mpi_context.rank);
  }

  // Reduce the number of words to the master process
  int partial_size = result.data.size();
  int rc_reduce = MPI_Reduce(&partial_size, &final_words_len, 1, MPI_INT, MPI_SUM, 0, mpi_context.comm);
  exit_on_fail(rc_reduce);

  // Reduce the dictionaries.word vector into the final_words vector using the custom MPI_Op
  rc_reduce = MPI_Reduce(result.data.data(), final_words, result.data.size(), mpi_word_type, mpi_reduce_word_op, 0, mpi_context.comm);
  exit_on_fail(rc_reduce);

  if(mpi_context.rank == 0) {
    dictionary final_dict;

    for(int i = 0; i < final_words_len; ++i) {
      fprintf(stderr, "Process %d adding word %s\n", mpi_context.rank, final_words[i].ngram);
      if(strcmp(final_words[i].ngram, ")(") == 0) {
        fprintf(stderr, "Process %d adding word %s\n with coverage %zu", mpi_context.rank, final_words[i].ngram, final_words[i].coverage);
      }
      final_dict.add_word(final_words[i]);
    }

    fprintf(stderr, "Process %d writing final dictionary\n", mpi_context.rank);
    // generate the final dictionary
    // NOTE: we sort it for pretty-printing
    std::cout << "NGRAM COVERAGE" << std::endl;
    std::sort(std::begin(final_dict.data), std::end(final_dict.data), word_coverage_gt_comparator{});
    final_dict.write(std::cout);
  }

  // Put a barrier to make sure that all processes have finished
  rc_barrier = MPI_Barrier(mpi_context.comm);
  exit_on_fail(rc_barrier);

  fprintf(stderr, "Process %d finished\n", mpi_context.rank);

  int rc_op_free = MPI_Op_free(&mpi_reduce_word_op);
  exit_on_fail(rc_op_free);

  int rc_comm_free = MPI_Comm_free(&mpi_context.comm);
  exit_on_fail(rc_comm_free);

  int rc_string_type_free = MPI_Type_free(&mpi_string_type);
  exit_on_fail(rc_string_type_free);

  int rc_word_type_free = MPI_Type_free(&mpi_word_type);
  exit_on_fail(rc_word_type_free);

  // finalize MPI
  int rc_finalize = MPI_Finalize();
  exit_on_fail(rc_finalize);

  return EXIT_SUCCESS;
}