#include <mpi.h>
#include <omp.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <unordered_set>
#include <vector>

#include "mpi_error_check.hpp"

#define MAX_LINE_LENGTH 1024

// set the maximum size of the ngram
static constexpr size_t max_pattern_len = 3;
static constexpr size_t max_dictionary_size = 128;
static_assert(max_pattern_len > 1, "The pattern must contain at least one character");
static_assert(max_dictionary_size > 1, "The dictionary must contain at least one element");

// global variables that hold the message tags
const int tag_size =
    0;  // tag to send the num of lines, such that each process can allocate a recvbuf of the right size

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
  if (provided_thread_level < MPI_THREAD_SINGLE) {
    std::cerr << "The MPI implementation does not support multiple threads" << std::endl;
    return EXIT_FAILURE;
  }

  // get the MPI context
  mpi_context_type mpi_context = mpi_context_type();

  // create and commit the MPI_Datatype for the string
  MPI_Datatype mpi_string_type;
  int rc_type = MPI_Type_contiguous(max_pattern_len + 1, MPI_CHAR, &mpi_string_type);
  exit_on_fail(rc_type);
  rc_type = MPI_Type_commit(&mpi_string_type);
  exit_on_fail(rc_type);

  // This is the vector of lines that will be read from the standard input by the master process
  std::vector<std::string> lines;

  // Total number of lines read from the standard input
  int num_chars;

  // This is the buffer that contains the lines read from the standard input
  std::string database;  // reserve 200MB
  database.reserve(209715200);

  if (mpi_context.rank == 0) {
    // Read The input from the standard input
    std::cerr << "Reading the molecules from the standard input ..." << std::endl;

    for (std::string line; std::getline(std::cin, line); /* automatically handled */) {
      lines.push_back(std::move(line));
    }

    fprintf(stderr, "Process %d read %ld lines\n", mpi_context.rank, lines.size());

    for (std::string line : lines) {
      for (const auto character : line) {
        database.push_back(character);
      }
    }

    num_chars = database.size();

    // Send it to the other processes
    for (int i = 1; i < mpi_context.size; ++i) {
      int rc_send = MPI_Send(&num_chars, 1, MPI_INT, i, tag_size, mpi_context.comm);
      exit_on_fail(rc_send);
    }
  } else {
    int rc_recv = MPI_Recv(&num_chars, 1, MPI_INT, 0, tag_size, mpi_context.comm, MPI_STATUS_IGNORE);
    exit_on_fail(rc_recv);
    fprintf(stderr, "Process %d knows that the database has %d chars\n", mpi_context.rank, num_chars);
  }

  // Master broadcast the database to the other processes
  int rc_bcast = MPI_Bcast(&database[0], num_chars, MPI_CHAR, 0, mpi_context.comm);
  exit_on_fail(rc_bcast);

  fprintf(stderr, "Process %d received database\n", mpi_context.rank);

  int rc_barrier = MPI_Barrier(mpi_context.comm);
  exit_on_fail(rc_barrier);  // here all processes have the same lines vector

  // Now recvbuf is ready to be used and contains the lines to be processed
  std::unordered_set<char> alphabet_builder;

  // compute the alphabet
  for (int i = 0; i < num_chars; ++i) {
    alphabet_builder.insert(database[i]);
  }

  // put the alphabet in a container with random access capabilities
  std::vector<char> alphabet;
  alphabet_builder.reserve(alphabet_builder.size());
  std::for_each(std::begin(alphabet_builder), std::end(alphabet_builder),
                [&alphabet](const auto character) { alphabet.push_back(character); });

  rc_barrier = MPI_Barrier(mpi_context.comm);
  exit_on_fail(rc_barrier);

  fprintf(stderr, "Process %d alphabet size: %zu\n", mpi_context.rank, alphabet.size());

  // declare the dictionary that holds all the ngrams with the greatest coverage of the dictionary
  dictionary result;

  std::size_t total_words;

  std::size_t start_index[max_pattern_len];
  std::size_t end_index[max_pattern_len];
  if (mpi_context.rank == 0) {
    // precompute the number of permutations according to the number of characters
    auto permutations = std::vector(max_pattern_len, alphabet.size());
    for (std::size_t i{1}; i < permutations.size(); ++i) {
      permutations[i] = alphabet.size() * permutations[i - std::size_t{1}];
    }

    // For each ngram size compute start and end index of the words to be processed
    std::size_t start;
    std::size_t end;
    for (std::size_t ngram_size{1}; ngram_size <= max_pattern_len; ++ngram_size) {
      int offset = 0;
      int total_words = permutations[ngram_size - std::size_t{1}];

      // i == 0 is the master process
      start_index[ngram_size - 1] = offset;
      end_index[ngram_size - 1] = offset + total_words / mpi_context.size;
      offset = end_index[ngram_size - 1];
      if (0 < total_words % mpi_context.size) {
        ++end_index[ngram_size - 1];
        ++offset;
      }

      for (int i = 1; i < mpi_context.size; ++i) {
        start = offset;
        end = offset + total_words / mpi_context.size;
        if (i < total_words % mpi_context.size) {
          ++end;
        }
        offset = end;
        int rc_send = MPI_Send(&start, 1, MPI_INT, i, tag_size, mpi_context.comm);
        exit_on_fail(rc_send);
        rc_send = MPI_Send(&end, 1, MPI_INT, i, tag_size, mpi_context.comm);
        exit_on_fail(rc_send);
      }
    }
  } else {
    for (std::size_t ngram_size{1}; ngram_size <= max_pattern_len; ++ngram_size) {
      int rc_recv = MPI_Recv(&start_index[ngram_size - 1], 1, MPI_INT, 0, tag_size, mpi_context.comm,
                             MPI_STATUS_IGNORE);
      exit_on_fail(rc_recv);
      rc_recv =
          MPI_Recv(&end_index[ngram_size - 1], 1, MPI_INT, 0, tag_size, mpi_context.comm, MPI_STATUS_IGNORE);
      exit_on_fail(rc_recv);
    }
  }

  fprintf(stderr, "Process %d computing from %zu to %zu words of ngram_size 1\n", mpi_context.rank,
          start_index[0], end_index[0]);
  fprintf(stderr, "Process %d computing from %zu to %zu words of ngram_size 2\n", mpi_context.rank,
          start_index[1], end_index[1]);
  fprintf(stderr, "Process %d computing from %zu to %zu words of ngram_size 3\n", mpi_context.rank,
          start_index[2], end_index[2]);

  // Now each process has the number of words to be processed, we can split the work
  for (std::size_t ngram_size{1}; ngram_size <= max_pattern_len; ++ngram_size) {
    for (std::size_t word_index = start_index[ngram_size - 1]; word_index < end_index[ngram_size - 1];
         ++word_index) {
      // compose the ngram
      word current_word;
      memset(current_word.ngram, '\0', max_pattern_len + 1);
      for (std::size_t character_index{0}, remaining_size = word_index; character_index < ngram_size;
           ++character_index, remaining_size /= alphabet.size()) {
        current_word.ngram[character_index] = alphabet[remaining_size % alphabet.size()];
      }
      current_word.size = ngram_size;

      // evaluate the coverage and add the word to the dictionary
      current_word.coverage = count_coverage(database.c_str(), current_word.ngram);
      result.add_word(current_word);
    }
  }

  int rc_reduce_barrier = MPI_Barrier(mpi_context.comm);
  exit_on_fail(rc_reduce_barrier);

  fprintf(stderr, "Process %d finished computing, dict size: %zu\n", mpi_context.rank, result.data.size());
  // Now each process has a dictionary with the ngrams with the greatest coverage, we need to reduce them to
  // the master process summing the coverage of the same ngrams and then the master process will compute the
  // final dictionary

  // Gather the vector of words of each process dictionary into a vector of dictionaries on the master process
  // Padding the vector of words with empty words.

  // Collect the maximum size and the sum of the sizes of the dictionaries

  int size_max;
  int size_sum;
  int size = result.data.size();

  int rc_reduce = MPI_Allreduce(&size, &size_max, 1, MPI_INT, MPI_MAX, mpi_context.comm);
  exit_on_fail(rc_reduce);
  rc_reduce = MPI_Allreduce(&size, &size_sum, 1, MPI_INT, MPI_SUM, mpi_context.comm);
  exit_on_fail(rc_reduce);

  rc_barrier = MPI_Barrier(mpi_context.comm);
  exit_on_fail(rc_barrier);

  fprintf(stderr, "Process %d size_max: %d, size_sum: %d\n", mpi_context.rank, size_max, size_sum);

  std::size_t partial_coverages[size_max];

  int i;
  for (i = 0; i < result.data.size(); ++i) {
    partial_coverages[i] = result.data[i].coverage;
  }
  for (; i < size_max; ++i) {  // padding
    partial_coverages[i] = 0;
  }

  char partial_ngrams[size_max][max_pattern_len + 1];

  for (i = 0; i < result.data.size(); ++i) {
    strcpy(partial_ngrams[i], result.data[i].ngram);
  }
  for (; i < size_max; ++i) {  // padding
    memset(partial_ngrams[i], '\0', max_pattern_len + 1);
  }

  // recvbuf will contain the gathered coverages and ngrams
  std::size_t coverages[size_sum];
  char ngrams[size_sum][max_pattern_len + 1];

  rc_barrier = MPI_Barrier(mpi_context.comm);
  exit_on_fail(rc_barrier);

  // gather the coverages and ngrams into the recvbuf
  int rc_gather = MPI_Gather(partial_coverages, size_max, MPI_UNSIGNED_LONG, coverages, size_max,
                             MPI_UNSIGNED_LONG, 0, mpi_context.comm);
  exit_on_fail(rc_gather);
  rc_gather = MPI_Gather(partial_ngrams, size_max, mpi_string_type, ngrams, size_max, mpi_string_type, 0,
                         mpi_context.comm);
  exit_on_fail(rc_gather);

  rc_barrier = MPI_Barrier(mpi_context.comm);
  exit_on_fail(rc_barrier);

  if (mpi_context.rank == 0) {
    fprintf(stderr, "Process %d writing final dictionary\n", mpi_context.rank);

    // declare the final dictionary that will be used to store the reduced dictionaries
    dictionary final_dict;

    for (int i = 0; i < size_sum; ++i) {
      word current_word;
      memset(current_word.ngram, '\0', max_pattern_len + 1);
      strcpy(current_word.ngram, ngrams[i]);
      current_word.size = strlen(ngrams[i]);
      current_word.coverage = coverages[i];
      final_dict.add_word(current_word);
    }

    // generate the final dictionary
    // NOTE: we sort it for pretty-printing
    std::cout << "NGRAM COVERAGE" << std::endl;
    std::sort(std::begin(final_dict.data), std::end(final_dict.data), word_coverage_gt_comparator{});
    final_dict.write(std::cout);
  }

  // Put a barrier to make sure that all processes have finished
  rc_barrier = MPI_Barrier(mpi_context.comm);
  exit_on_fail(rc_barrier);

  fprintf(stderr, "Process %d terminated\n", mpi_context.rank);

  int rc_comm_free = MPI_Comm_free(&mpi_context.comm);
  exit_on_fail(rc_comm_free);

  int rc_string_type_free = MPI_Type_free(&mpi_string_type);
  exit_on_fail(rc_string_type_free);

  // finalize MPI
  int rc_finalize = MPI_Finalize();
  exit_on_fail(rc_finalize);

  return EXIT_SUCCESS;
}