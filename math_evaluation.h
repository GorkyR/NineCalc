#pragma once
#include "grs.h"
#include "memory_arena.h"
#include "utf32_string.h"

#include <cmath>

enum class Token_Type
{
	Invalid,
	Number,
	Identifier,
	Operator,
	Parenthesis,
	Separator,
	End
};

struct Token
{
	Token_Type type;
	UTF32_String text;
};

struct Token_List
{
	Token *data;
	u64   count;
	Token &operator[](u64 index);
};

enum class Precedence
{
	Invalid,
	Addition,
	Subtraction,
	Multiplication,
	Division,
	Exponentiation,
	Annex,
	Parenthesis
};

struct AST
{
	Token      token;
	u32        consumed;
	AST        *left;
	AST        *right;
	Precedence precedence;
	bool       invalid;
};

struct Result
{
	bool   valid;
	f64    value;
};

struct Context
{
	UTF32_String *variable_names;
	f64          *values;
	u64          variable_count;
	u64          capacity;

	Result operator[](UTF32_String);
};

typedef Result Math_Function(f64*, u32);

internal Token_List tokenize_expression(Memory_Arena*, UTF32_String);
internal AST        *parse_tokens      (Memory_Arena*, Token_List, Token_Type = (Token_Type)0);
internal Result     evaluate_tree      (AST*, Context* =0);
internal Result     evaluate_expression(Memory_Arena*, UTF32_String, Context* =0);

internal Context make_context(Memory_Arena *, u64);
internal void add_or_update_variable(Context*, UTF32_String, f64);

//--------------------------------------------------

internal bool is_number     (u32 codepoint) { return codepoint >= '0' && codepoint <= '9'; }
internal bool is_letter     (u32 codepoint) { return (codepoint >= 'A' && codepoint <= 'Z') || (codepoint >= 'a' && codepoint <= 'z'); }
internal bool is_parenthesis(u32 codepoint) { return codepoint == '(' || codepoint == ')'; }
internal bool is_separator  (u32 codepoint) { return codepoint == ','; }

internal bool is_number_or_identifier(Token token) { return token.type == Token_Type::Number || token.type == Token_Type::Identifier; }
internal bool is_open_parenthesis    (Token token) { return token.type == Token_Type::Parenthesis && token.text[0] == '('; }
internal bool is_prefix_operator     (Token);
internal bool is_suffix_operator     (Token);
internal bool is_end_of_expression   (Token, Token_Type);

internal bool starts_with_operator(UTF32_String);

internal void  consume_whitespace       (UTF32_String*);
internal Token consume_number_token     (UTF32_String*);
internal Token consume_identifier_token (UTF32_String*);
internal Token consume_operator_token   (UTF32_String*);
internal Token consume_parenthesis_token(UTF32_String*);
internal Token consume_separator_token  (UTF32_String*);
internal Token consume_invalid_token    (UTF32_String*);

internal AST*   make_node         (Memory_Arena*);
internal AST*   make_term_node    (Memory_Arena*, Token);
internal AST*   make_operator_node(Memory_Arena*, Token, AST *left_node);
internal AST*   make_operator_node(Memory_Arena*, Token, AST *left_node, AST *right_node);
internal Token* make_end_token    (Memory_Arena*);

internal Token_List tokens_from(Token_List, u64);

internal AST* parse_prefixed_term(Memory_Arena*, Token_List);
internal AST* parse_function_call(Memory_Arena*, Token, Token_List);

internal Result factorial(f64);

internal Math_Function exponentiation;
internal Math_Function square_root;
internal Math_Function absolute_value;

internal Token_List
tokenize_expression(Memory_Arena *arena, UTF32_String input)
{
	Token_List tokens = {};
	tokens.data = cast_tail(arena, Token);

	consume_whitespace(&input);
	while (input.length)
	{
		Token *token = allocate_struct(arena, Token);

		if (is_number(input[0]) || input[0] == '.')
			*token = consume_number_token(&input);
		else if (is_letter(input[0]) || input[0] == '_')
			*token = consume_identifier_token(&input);
		else if (starts_with_operator(input))
			*token = consume_operator_token(&input);
		else if (is_parenthesis(input[0]))
			*token = consume_parenthesis_token(&input);
		else if (is_separator(input[0]))
			*token = consume_separator_token(&input);
		else
			*token = consume_invalid_token(&input);

		++tokens.count;

		consume_whitespace(&input);
	}
	make_end_token(arena);
	++tokens.count;

	return tokens;
}

AST *parse_tokens(Memory_Arena *arena, Token_List tokens, Token_Type expression_type)
{
	AST *node = 0;
	if (tokens.count)
	{
		Token token = tokens[0];

		if (token.type == Token_Type::Number)
			node = make_term_node(arena, token);
		else if (token.type == Token_Type::Identifier)
		{
			if (is_open_parenthesis(tokens[1]))
			{
				node = parse_function_call(arena, token, tokens_from(tokens, 2));
			}
			else
				node = make_term_node(arena, token);
		}
		else if (is_open_parenthesis(token))
		{
			node = parse_tokens(arena, tokens_from(tokens, 1), Token_Type::Parenthesis);
			node->consumed += 2;
			node->precedence = Precedence::Parenthesis;
		}
		else if (is_prefix_operator(token))
			node = parse_prefixed_term(arena, tokens);
		else
		{
			if (!node)
				node = make_node(arena);
			node->invalid = true;
		}

		token = tokens[node->consumed];
		if (is_suffix_operator(token))
		{
			AST *left_node = node;
			node = make_operator_node(arena, token, left_node);
			node->precedence = Precedence::Annex;
			token = tokens[node->consumed];
		}

		if (token.type == Token_Type::Operator)
		{
			AST *left_node = node;
			node = make_operator_node(arena, token, left_node,
				parse_tokens(arena, tokens_from(tokens, left_node->consumed + 1)));
			// rotate tree to comply with order of operations
			if (node->right->token.type == Token_Type::Operator &&
				node->right->precedence <= node->precedence)
			{
				u32 consumed = node->consumed;
				AST *node_right_left = node->right->left;
				node->right->left = node;
				node = node->right;
				node->left->right = node_right_left;
				node->consumed = consumed;
			}
		}
		else if (!is_end_of_expression(token, expression_type))
		{
			if (!node)
				node = make_node(arena);
			node->invalid = true;
		}
	}
	return node;
}

char* function_names[]     = { "pow", "sqrt", "abs" };
Math_Function *functions[] = { exponentiation, square_root, absolute_value };

internal Result
evaluate_tree(AST *tree, Context *context)
{
	Result result = {};
	if (tree)
	{
		Token token = tree->token;
		if (token.type == Token_Type::Number)
			result = { true, parse_float(token.text) };
		else if (token.type == Token_Type::Identifier)
		{
			if (tree->left)
			{
				Math_Function *function = 0;
				for (u64 i = 0; i < array_count(functions); ++i)
				{
					if (strings_are_equal(function_names[i], token.text))
					{
						function = functions[i];
						break;
					}
				}
				if (function)
				{
					f64 parameters[10] = {};
					u32 parameter_count = 0;

					AST *node = tree;
					while (parameter_count <= 10)
					{
						Result parameter_result = evaluate_tree(node->left, context);
						if (parameter_result.valid)
							parameters[parameter_count++] = parameter_result.value;
						else
							return result;

						if (node->right && node->right->token.type == Token_Type::Separator)
							node = node->right;
						else break;
					}
					if (!node->right)
						result = function(&parameters[0], parameter_count);
				}
			}
			else
				result = (*context)[token.text];
		}
		else if (token.type == Token_Type::Operator)
		{
			if (token.text[0] == ':')
			{
				Token left_token = tree->left->token;
				if (left_token.type == Token_Type::Identifier)
				{
					result = evaluate_tree(tree->right, context);
					if (result.valid)
						add_or_update_variable(context, left_token.text, result.value);
				}
			}
			else
			{
				Result left_result = evaluate_tree(tree->left, context);
				if (left_result.valid)
				{
					Result right_result = evaluate_tree(tree->right, context);
					if (right_result.valid)
					{
						result.valid = true;
						if (token.text[0] == '+')
							result.value = left_result.value + right_result.value;
						else if (token.text[0] == '-')
							result.value = left_result.value - right_result.value;
						else if (token.text[0] == '*')
							result.value = left_result.value * right_result.value;
						else if (token.text[0] == '/')
						{
							if (right_result.value != 0)
								result.value = left_result.value / right_result.value;
							else
								result.valid = false;
						}
						else if (token.text[0] == '^')
							result.value = pow(left_result.value, right_result.value);
					}
					else
					{
						if (tree->precedence >= Precedence::Annex)
						{
							if (token.text[0] == '-') 
								result = { true, -left_result.value };
							else if (token.text[0] == '!')
								result = factorial(left_result.value);
						}
					}
				}
			}
		}
	}
	if (result.valid && isnan(result.value))
		result.valid = false;
	return result;
}

internal Result
evaluate_expression(Memory_Arena *arena, UTF32_String expression, Context *context)
{
	Token_List tokens = tokenize_expression(arena, expression);
	AST *tree = parse_tokens(arena, tokens);
	Result result = evaluate_tree(tree, context);
	return result;
}

Result Context::operator[](UTF32_String variable)
{
	Result result = {};
	for (u64 i = 0; i < this->variable_count; ++i)
	{
		if (strings_are_equal(this->variable_names[i], variable))
		{
			result = { true, this->values[i] };
			break;
		}
	}
	return result;
}

Token &Token_List::operator[](u64 index)
{
	if (index < this->count)
		return this->data[index];
	else
	{
		persistent Token end_token = {Token_Type::End};
		return end_token;
	}
}

internal bool is_whitespace(u32 codepoint) { return codepoint == ' ' || codepoint == '\n' || codepoint == '\t' ; }

internal void
consume_whitespace(UTF32_String *input)
{
	while (input->length && is_whitespace(*(input->data)))
	{
		++(input->data);
		--(input->length);
	}
}

internal bool
starts_with_operator(UTF32_String input)
{
	return(input[0] == '+' ||
		   input[0] == '-' ||
		   input[0] == '*' ||
		   input[0] == '/' ||
		   input[0] == '^' ||
		   input[0] == '!' ||
		   input[0] == ':');
}

internal bool is_valid_number(u32 codepoint)   { return is_number(codepoint) || codepoint == '_' || codepoint == '.'; }
internal bool is_valid_variable(u32 codepoint) { return is_letter(codepoint) || is_number(codepoint) || codepoint == '_'; }

internal Token
consume_number_token(UTF32_String *input)
{
	Token token = { Token_Type::Number };

	bool has_decimal = false;
	u64 i = 0;
	while (i < input->length && is_valid_number(input->data[i]))
	{
		u32 codepoint = input->data[i];
		if (codepoint == '.')
		{
			if (has_decimal)
			{
				token.type = Token_Type::Invalid;
			}
			has_decimal = true;
		}
		++i;
	}
	token.text = substring(*input, 0, i);
	*input = substring(*input, i);

	return token;
}

internal Token
consume_identifier_token(UTF32_String *input)
{
	Token token = { Token_Type::Identifier };

	u64 i = 0;
	while (i < input->length && is_valid_variable(input->data[i]))
		++i;
	token.text = substring(*input, 0, i);
	*input = substring(*input, i);

	return token;
}

internal Token 
consume_operator_token(UTF32_String *input)
{
    Token token = { Token_Type::Operator };
    // for now, all operators are one character, so...
	token.text = substring(*input, 0, 1);
	*input = substring(*input, 1);

	return token;
}

internal Token 
consume_parenthesis_token(UTF32_String *input)
{
    Token token = { Token_Type::Parenthesis };

	token.text = substring(*input, 0, 1);
	*input = substring(*input, 1);

	return token;
}

internal Token 
consume_separator_token(UTF32_String *input)
{
    Token token = { Token_Type::Separator };

	token.text = substring(*input, 0, 1);
	*input = substring(*input, 1);

	return token;
}

internal Token 
consume_invalid_token(UTF32_String *input)
{
	Token token = { Token_Type::Invalid };

	u64 i = 0;
	while (i < input->length && (*input)[i] != ' ')
		++i;
	token.text = substring(*input, 0, i);
	*input = substring(*input, i);

	return token;
}

internal Token *
make_end_token(Memory_Arena *arena)
{
	Token *token = allocate_struct(arena, Token);
	*token = Token{ Token_Type::End };
	return token;
}

internal bool is_prefix_operator(Token token)
{
	// only '-' as a prefix operator so far
	return token.type == Token_Type::Operator && token.text[0] == '-';
}
internal bool is_suffix_operator(Token token)
{
	// only '!' as a sufix operator so far
	return token.type == Token_Type::Operator && token.text[0] == '!';
}

internal bool is_end_of_expression(Token token, Token_Type expression_type)
{
	if (token.type == Token_Type::End)
		return true;

	bool is_close_parenthesis = token.type == Token_Type::Parenthesis && token.text[0] == ')';
	if (expression_type == Token_Type::Parenthesis)
		return is_close_parenthesis;
	else if (expression_type == Token_Type::Identifier)
		return is_close_parenthesis || token.type == Token_Type::Separator;
	else
		return false;
}

internal Token_List tokens_from(Token_List list, u64 offset)
{
	return Token_List{ list.data + offset, list.count - offset };
}

internal AST *make_node(Memory_Arena *arena)
{
	AST *node = allocate_struct(arena, AST);
	*node = {};
	return node;
}

internal AST *make_term_node(Memory_Arena *arena, Token token)
{
	AST *node = allocate_struct(arena, AST);
	*node = { token, 1 };
	return node;
}

internal AST *make_operator_node(Memory_Arena *arena, Token token, AST *left)
{
	AST *node = allocate_struct(arena, AST);
	*node = { token, left->consumed + 1, left };
	return node;
}

internal AST *make_operator_node(Memory_Arena *arena, Token token, AST *left, AST *right)
{
	AST *node = allocate_struct(arena, AST);
	*node = { token, left->consumed + right->consumed + 1, left, right };
	switch(token.text[0])
	{
		case '+': node->precedence = Precedence::Addition;       break;
		case '-': node->precedence = Precedence::Subtraction;    break;
		case '*': node->precedence = Precedence::Multiplication; break;
		case '/': node->precedence = Precedence::Division;       break;
		case '^': node->precedence = Precedence::Exponentiation; break;
	}
	return node;
}

internal AST *parse_prefixed_term(Memory_Arena *arena, Token_List tokens)
{
	AST *term = 0;
	if (is_number_or_identifier(tokens[1]))
		term = make_term_node(arena, tokens[1]);
	else if (is_open_parenthesis(tokens[1]))
	{
		term = parse_tokens(arena, tokens_from(tokens, 2));
		term->consumed += 2;
		term->precedence = Precedence::Parenthesis;
	}

	AST* prefixed_term;
	if (term)
	{
		prefixed_term = make_operator_node(arena, tokens[0], term);
		prefixed_term->precedence = Precedence::Annex;
	}
	else
	{
		prefixed_term = make_term_node(arena, tokens[0]);
		prefixed_term->invalid = true;
	}
	return prefixed_term;
}

// pow(2, 2) -> pow
//              / \
//             2   ,
//                /
//               2 

internal AST *parse_function_call(Memory_Arena *arena, Token identifier, Token_List tokens)
{
	AST *node = make_term_node(arena, identifier);

	u32 next = 0;
	AST *current_node = node;
	while (true)
	{
		current_node->left = parse_tokens(arena, tokens_from(tokens, next), Token_Type::Identifier);
		next += current_node->left->consumed;
		Token next_token = tokens[next++];
		if (next_token.type == Token_Type::Separator)
		{
			current_node->right = make_term_node(arena, next_token);
			current_node = current_node->right;
		} else 
			break;
	}
	node->consumed += next;

	return node;
}

internal Result
factorial(f64 input)
{
	Result result = {};
	s64 number = (s64)input;
	if (number >= 0)
	{
		if (number < 2)
			result = { true, 1 };
		else
		{
			f64 value = 1;
			for (s64 i = 2; i <= number; ++i)
				value *= i;
			result = { true, value };
		}
	}
	return result;
}

internal Result
exponentiation(f64* parameters, u32 parameter_count)
{
	Result result = {};
	if (parameter_count == 2)
		result = { true, pow(parameters[0], parameters[1]) };
	return result;
}

internal Result
square_root(f64* parameters, u32 parameter_count)
{
	Result result = {};
	if (parameter_count == 1)
		result = { true, sqrt(parameters[0]) };
	return result;
}

internal Result
absolute_value(f64* parameters, u32 parameter_count)
{
	Result result = {};
	if (parameter_count == 1)
		result = { true, abs(parameters[0]) };
	return result;
}

void add_or_update_variable(Context *context, UTF32_String variable, f64 value)
{
	bool existed = false;
	for (u64 i = 0; i < context->variable_count; ++i)
	{
		if (strings_are_equal(context->variable_names[i], variable))
		{
			context->values[i] = value;
			existed = true;
			break;
		}
	}
	if (!existed)
	{
		assert(context->variable_count < context->capacity);
		context->variable_names [context->variable_count] = variable;
		context->values[context->variable_count] = value;
		++context->variable_count;
	}
}

Context make_context(Memory_Arena *arena, u64 capacity)
{
	Context context = {};
	context.variable_names = allocate_array(arena, UTF32_String, capacity);
	context.values         = allocate_array(arena, f64, capacity);
	context.capacity       = capacity;
	return context;
}
