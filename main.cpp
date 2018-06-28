#include <iostream>
#include <vector>
#include "variant_ptr.h"

using namespace lius_tools;

struct Rock {};
struct Paper {};
struct Scissors {};

struct GetDescription {
  static std::string visit(Rock rock) {
    return "Rock, a very solid move.";
  }
  static std::string visit(Paper paper) {
    return "Paper, a very elusive move.";
  }
  static std::string visit(Scissors scissors) {
    return "Scissors, a very sharp move.";
  }
};

struct LosesTo {
  static bool visit(Rock rock, Paper paper) {
    return true;
  }

  static bool visit(Paper paper, Scissors scissors) {
    return true;
  }

  static bool visit(Scissors scissors, Rock rock) {
    return true;
  }

  template <typename A, typename B>
  static bool visit(A a, B b) {
    return false;
  }
};

using HandPtr = variant_ptr<Rock, Paper, Scissors>;

int main(int argc, char *argv[])
{
  Rock rock;
  Paper paper;
  Scissors scissors;

  LosesTo loses_to;
  GetDescription get_description;

  std::vector<HandPtr> alices_moves {
    HandPtr(&paper), HandPtr(&scissors), HandPtr(&paper), HandPtr(&rock)
  };
  std::vector<HandPtr> bobs_moves {
    HandPtr(&paper), HandPtr(&rock), HandPtr(&paper), HandPtr(&scissors)
  };

  for (size_t round_idx = 0; round_idx < alices_moves.size(); ++round_idx) {
    std::cout << "Round " << round_idx << "-----------" << std::endl;

    HandPtr alices_hand = alices_moves[round_idx];
    HandPtr bobs_hand = bobs_moves[round_idx];

    std::cout << "\tAlice throws " << alices_hand.visit(get_description) << std::endl;
    std::cout << "\tBob throws " << bobs_hand.visit(get_description) << std::endl;

    bool alice_loses_to_bob =
        apply_multi_visitor<2>(loses_to,
                               alices_hand,
                               bobs_hand);
    bool bob_loses_to_alice =
        apply_multi_visitor<2>(loses_to,
                               bobs_hand,
                               alices_hand);

    if (alice_loses_to_bob) {
      std::cout << "\tAlice loses to Bob" << std::endl;
    }
    else if (bob_loses_to_alice) {
      std::cout << "\tBob loses to Alice" << std::endl;
    }
    else {
      std::cout << "\tAlice and Bob tie" << std::endl;
    }
  }

  std::cout << "Game complete!" << std::endl;
  return 0;
}
