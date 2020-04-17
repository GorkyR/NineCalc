#include "string.h"
#include "calculator.h"
#include <cstdlib>
#include <cstdio>

#undef assert
#define assert(expression) { if (!(expression)) { failed = true; printf("\n  - Assertion failed: \"" #expression "\" (line %d)", __LINE__); } }

#define begin_test            { failed = false; printf("TEST %2d (line %4d):", test_count, __LINE__); } try
//#define begin_test(test_name) printf("TEST %s (line %4d):", test_name, test_count, __LINE__); try {
#define end_test              catch(...) { failed = true; printf("\n  - Exception."); } if (failed) { printf("\n  FAILED\n"); ++test_count; } else { printf(" PASSED\n"); ++test_count; }

int main()
{
	Memory arena = {0, mebibytes(1), 0};
	arena.contents = malloc(arena.size);

	u32 test_count = 1;
	bool32 failed = false;

	begin_test {
		// "1 + 23" ->
		// num(1), op(+), num(23) ->
		// node(+, node(1), node(23))

		String text = make_string_from_chars(&arena, "1 + 23");
		Token_List tokens = tokenize(&arena, text);
		assert(tokens.count == 4);
		assert(tokens[0].type == Token_Type::Number);
		assert(tokens[1].type == Token_Type::Operator);
		assert(tokens[2].type == Token_Type::Number);

		AST * ast = parse(&arena, tokens);
		assert(ast->token.type == Token_Type::Operator);
		assert(ast->left);
			assert(ast->left->token.type == Token_Type::Number);
			assert(!ast->left->left);
			assert(!ast->left->right);
		assert(ast->right);
			assert(ast->right->token.type == Token_Type::Number);
			assert(!ast->right->left);
			assert(!ast->right->right);
	} end_test;

	begin_test {
		// "1 + 23 * 456" ->
		// num(1), op(+), num(23), op(*), num(456) ->
		// node(+, node(1), node(*, node(23), node(546)))

		String text = make_string_from_chars(&arena, "1 + 23 * 456");
		Token_List tokens = tokenize(&arena, text);
		assert(tokens.count == 6);
		assert(tokens[0].type == Token_Type::Number);
		assert(tokens[1].type == Token_Type::Operator);
		assert(tokens[2].type == Token_Type::Number);
		assert(tokens[3].type == Token_Type::Operator);
		assert(tokens[4].type == Token_Type::Number);

		AST * ast = parse(&arena, tokens);
		assert(ast->token.type == Token_Type::Operator);
		assert(ast->left);
			assert(ast->left->token.type == Token_Type::Number);
			assert(!ast->left->left);
			assert(!ast->left->right);
		assert(ast->right);
			assert(ast->right->token.type == Token_Type::Operator);
			assert(ast->right->left);
				assert(ast->right->left->token.type == Token_Type::Number);
				assert(!ast->right->left->left);
				assert(!ast->right->left->right);
			assert(ast->right->right);
				assert(ast->right->right->token.type == Token_Type::Number);
				assert(!ast->right->right->left);
				assert(!ast->right->right->right);
	} end_test;

	begin_test {
		// "(1 + 23) * 456" ->
		// paren((), num(1), op(+), num(23), paren()), op(*), num(456) ->
		// node(*, node(+, node(1), node(23)), node(456))

		String text = make_string_from_chars(&arena, "(1 + 23) * 456");
		Token_List tokens = tokenize(&arena, text);
		assert(tokens.count == 8);
		assert(tokens[0].type == Token_Type::Parenthesis);
		assert(tokens[1].type == Token_Type::Number);
		assert(tokens[2].type == Token_Type::Operator);
		assert(tokens[3].type == Token_Type::Number);
		assert(tokens[4].type == Token_Type::Parenthesis);
		assert(tokens[5].type == Token_Type::Operator);
		assert(tokens[6].type == Token_Type::Number);

		AST* ast = parse(&arena, tokens);
		assert(ast->token.type == Token_Type::Operator);
		assert(ast->left);
			assert(ast->left->token.type == Token_Type::Operator);
			assert(ast->left->left);
				assert(ast->left->left->token.type == Token_Type::Number);
				assert(!ast->left->left->left);
				assert(!ast->left->left->right);
			assert(ast->left->right);
				assert(ast->left->right->token.type == Token_Type::Number);
				assert(!ast->left->right->left);
				assert(!ast->left->right->right);
		assert(ast->right);
			assert(ast->right->token.type == Token_Type::Number);
			assert(!ast->right->left);
			assert(!ast->right->right);
	} end_test;

	begin_test {
		// "((1 + 23) * 456) / 7890" ->
		// paren((), paren((), num(1), op(+), num(23), paren()), op(*), num(456), paren()), op(/), num(7890) ->
		// node(/, node(*, node(+, node(1), node(23)), node(456)), node(7890))

		String text = make_string_from_chars(&arena, "((1 + 23) * 456) / 7890");
		Token_List tokens = tokenize(&arena, text);
		assert(tokens.count == 12);
		assert(tokens[ 0].type == Token_Type::Parenthesis);
		assert(tokens[ 1].type == Token_Type::Parenthesis);
		assert(tokens[ 2].type == Token_Type::Number	 );
		assert(tokens[ 3].type == Token_Type::Operator	 );
		assert(tokens[ 4].type == Token_Type::Number	 );
		assert(tokens[ 5].type == Token_Type::Parenthesis);
		assert(tokens[ 6].type == Token_Type::Operator	 );
		assert(tokens[ 7].type == Token_Type::Number	 );
		assert(tokens[ 8].type == Token_Type::Parenthesis);
		assert(tokens[ 9].type == Token_Type::Operator	 );
		assert(tokens[10].type == Token_Type::Number	 );

		AST* ast = parse(&arena, tokens);
		assert(ast->token.type == Token_Type::Operator);
		assert(ast->left);
			assert(ast->left->token.type == Token_Type::Operator);
			assert(ast->left->left);
				assert(ast->left->left->token.type == Token_Type::Operator);
				assert(ast->left->left->left);
					assert(ast->left->left->left->token.type == Token_Type::Number);
					assert(!ast->left->left->left->left );
					assert(!ast->left->left->left->right);
				assert(ast->left->left->right);
					assert(ast->left->left->right->token.type == Token_Type::Number);
					assert(!ast->left->left->right->left );
					assert(!ast->left->left->right->right);
			assert(ast->left->right);
				assert(ast->left->right->token.type == Token_Type::Number);
				assert(!ast->left->right->left);
				assert(!ast->left->right->right);
		assert(ast->right);
			assert(ast->right->token.type == Token_Type::Number);
			assert(!ast->right->left);
			assert(!ast->right->right);
	} end_test;

	begin_test {
		// "prev + 5" ->
		// ident(prev), op(+), num(5) ->
		// node(+, node(prev), node(5))

		String text = make_string_from_chars(&arena, "prev + 5");
		Token_List tokens = tokenize(&arena, text);
		assert(tokens.count == 4);
		assert(tokens[0].type == Token_Type::Identifier);
		assert(tokens[1].type == Token_Type::Operator  );
		assert(tokens[2].type == Token_Type::Number    );

		AST* ast = parse(&arena, tokens);
		assert(ast->token.type == Token_Type::Operator);
		assert(ast->left);
			assert(ast->left->token.type == Token_Type::Identifier);
			assert(!ast->left->left);
			assert(!ast->left->right);
		assert(ast->right);
			assert(ast->right->token.type == Token_Type::Number);
			assert(!ast->right->left);
			assert(!ast->right->right);
	} end_test;

	begin_test {
		// "" -> invalid()

		String text = make_string_from_chars(&arena, "");
		Token_List tokens = tokenize(&arena, text);
		AST* ast = parse(&arena, tokens);
		Result result = evaluate(*ast);
		assert(!result.valid);
	} end_test;

	begin_test {
		// "27" -> int(27)

		String text = make_string_from_chars(&arena, "27");
		Token_List tokens = tokenize(&arena, text);
		AST* ast = parse(&arena, tokens);
		Result result = evaluate(*ast);
		assert(result.valid);
		assert(result.type == Number_Type::Integer);
		assert(result.value.integer == 27);
	} end_test;

	begin_test {
		// "0.5" -> real(0.5)

		String text = make_string_from_chars(&arena, "0.5");
		Token_List tokens = tokenize(&arena, text);
		AST* ast = parse(&arena, tokens);
		Result result = evaluate(*ast);
		assert(result.valid);
		assert(result.type == Number_Type::Real);
		assert(result.value.real == 0.5);
	} end_test;

	begin_test {
		// "1 + 2" -> int(3)

		String text = make_string_from_chars(&arena, "1 + 2");
		Token_List tokens = tokenize(&arena, text);
		AST* ast = parse(&arena, tokens);
		Result result = evaluate(*ast);
		assert(result.valid);
		assert(result.type == Number_Type::Integer);
		assert(result.value.integer == 3);
	} end_test;

	begin_test {
		// "1 + 2.3" -> real(3.3)

		String text = make_string_from_chars(&arena, "1 + 2.3");
		Token_List tokens = tokenize(&arena, text);
		AST* ast = parse(&arena, tokens);
		Result result = evaluate(*ast);
		assert(result.valid);
		assert(result.type == Number_Type::Real);
		assert(result.value.real == 3.3);
	} end_test;

	begin_test {
		//                        *                +    
		//                       / \              / \   
		// "1 * 20 + 300"  =>  10   +     =>     *   30 
		//                         / \          / \     
		//                       20   30      10   20  

		String text = make_string_from_chars(&arena, "1 * 20 + 300");
		Token_List tokens = tokenize(&arena, text);
		AST *ast = parse(&arena, tokens);
		assert(ast->token.type == Token_Type::Operator);
		assert(ast->token.precedence == Operator_Precedence::Addition);
		assert(ast->left);
			assert(ast->left->token.type == Token_Type::Operator);
			assert(ast->left->token.precedence == Operator_Precedence::Multiplication);
			assert(ast->left->left );
				assert(ast->left->left->token.type == Token_Type::Number);
				assert(!ast->left->left->left );
				assert(!ast->left->left->right);
			assert(ast->left->right);
				assert(ast->left->right->token.type == Token_Type::Number);
				assert(!ast->left->right->left );
				assert(!ast->left->right->right);
		assert(ast->right);
			assert(ast->right->token.type == Token_Type::Number);
			assert(!ast->right->left);
			assert(!ast->right->right);
	} end_test;

	begin_test {
		//                        -               +   
		//                       / \             / \  
		// "500 - 40 + 1"  =>  500  +    =>     -   1 
		//                         / \         / \    
		//                       40   1      500  40  

		Result result = evaluate(&arena, make_string_from_chars(&arena, "500 - 40 + 1"));
		assert(result.valid);
		assert(result.type == Number_Type::Integer);
		assert(result.value.integer == 461);
		assert(result.value.integer != 459);
	} end_test;
}