#pragma once
#include "grs.h"
#include "memory_arena.h"
#include "u32_string.h"

#include <cmath>

enum class Token_Type
{
	Invalid,
	Number,
	Variable,
	Operator,
	Parenthesis,
	End
};

struct Token
{
	Token_Type type;
	U32_String text;
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
	bool32     invalid;
};

struct Result
{
	bool32 valid;
	f64    value;
};

struct Context
{
	U32_String *variables;
	f64        *values;
	u64        count;
	u64        capacity;

	Result operator[](U32_String);
};

internal Token_List tokenize_expression(Memory_Arena*, U32_String);
internal AST *parse_tokens(Memory_Arena*, Token_List);
internal Result evaluate_tree(AST*, Context*);
internal Result evaluate_expression(Memory_Arena*, U32_String, Context*);

internal Context make_context(Memory_Arena *, u64);
internal void add_or_update_variable(Context*, U32_String, f64);

//--------------------------------------------------

Result Context::operator[](U32_String variable)
{
	Result result = {};
	for (u64 i = 0; i < this->count; ++i)
	{
		if (strings_are_equal(this->variables[i], variable))
		{
			result = { true, this->values[i] };
			break;
		}
	}
	return(result);
}

Token &Token_List::operator[](u64 index)
{
	if (index < this->count)
		return(this->data[index]);
	else
		return(this->data[this->count - 1]);
}

internal bool32 is_whitespace(u32 codepoint) { return(codepoint == ' ' || codepoint == '\n' || codepoint == '\t' ); }

internal void
consume_whitespace(U32_String *input)
{
	while (input->length && is_whitespace(*(input->data)))
	{
		++(input->data);
		--(input->length);
	}
}

internal bool32 is_number(u32 codepoint) { return(codepoint >= '0' && codepoint <= '9'); }
internal bool32 is_parenthesis(u32 codepoint) { return(codepoint == '(' || codepoint == ')'); }
internal bool32
is_letter(u32 codepoint)
{
	return((codepoint >= 'A' && codepoint <= 'Z') ||
		   (codepoint >= 'a' && codepoint <= 'z'));
}
internal bool32
starts_with_operator(U32_String input)
{
	return(input[0] == '+' ||
		   input[0] == '-' ||
		   input[0] == '*' ||
		   input[0] == '/' ||
		   input[0] == '^' ||
		   input[0] == '!');
}

internal bool32 is_valid_number(u32 codepoint)   { return(is_number(codepoint) || codepoint == '_' || codepoint == '.'); }
internal bool32 is_valid_variable(u32 codepoint) { return(is_letter(codepoint) || is_number(codepoint) || codepoint == '_'); }

internal Token
consume_number_token(U32_String *input)
{
	Token token = { Token_Type::Number };

	bool32 has_decimal = false;
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
consume_variable_token(U32_String *input)
{
	Token token = { Token_Type::Variable };

	u64 i = 0;
	while (i < input->length && is_valid_variable(input->data[i]))
		++i;
	token.text = substring(*input, 0, i);
	*input = substring(*input, i);

	return token;
}

internal Token 
consume_operator_token(U32_String *input)
{
    Token token = { Token_Type::Operator };
    // for now, all operators are one character, so...
	token.text = substring(*input, 0, 1);
	*input = substring(*input, 1);

	return token;
}

internal Token 
consume_parenthesis_token(U32_String *input)
{
    Token token = { Token_Type::Parenthesis };

	token.text = substring(*input, 0, 1);
	*input = substring(*input, 1);

	return token;
}

internal Token 
consume_invalid_token(U32_String *input)
{
	Token token = { Token_Type::Invalid };

	u64 i = 0;
	while (i < input->length && (*input)[i] != ' ')
		++i;
	token.text = substring(*input, 0, i);
	*input = substring(*input, i);

	return(token);
}

internal Token *
make_end_token(Memory_Arena *arena)
{
	Token *token = allocate_struct(arena, Token);
	*token = Token{ Token_Type::End };
	return(token);
}

internal Token_List
tokenize_expression(Memory_Arena *arena, U32_String input)
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
			*token = consume_variable_token(&input);
		else if (starts_with_operator(input))
			*token = consume_operator_token(&input);
		else if (is_parenthesis(input[0]))
			*token = consume_parenthesis_token(&input);
		else
			*token = consume_invalid_token(&input);

		++tokens.count;

		consume_whitespace(&input);
	}
	make_end_token(arena);
	++tokens.count;

	return(tokens);
}

bool32 is_number_or_variable(Token token) { return(token.type == Token_Type::Number || token.type == Token_Type::Variable); }
bool32 is_open_parenthesis(Token token) { return(token.type == Token_Type::Parenthesis && token.text[0] == '('); }
bool32 is_prefix_operator(Token token)
{
	// only '-' as a prefix operator so far
	return(token.type == Token_Type::Operator && token.text[0] == '-');
}
bool32 is_suffix_operator(Token token)
{
	// only '!' as a sufix operator so far
	return(token.type == Token_Type::Operator && token.text[0] == '!');
}
bool32 is_end_of_expression(Token token)
{
	return(token.type == Token_Type::End ||
		   (token.type == Token_Type::Parenthesis &&
		   	token.text[0] == ')'));
}

Token_List tokens_from(Token_List list, u64 offset)
{
	return(Token_List{ list.data + offset, list.count - offset });
}

AST *make_node(Memory_Arena *arena)
{
	AST *node = allocate_struct(arena, AST);
	*node = {};
	return(node);
}

AST *make_term_node(Memory_Arena *arena, Token token)
{
	AST *node = allocate_struct(arena, AST);
	*node = { token, 1 };
	return(node);
}

AST *make_operator_node(Memory_Arena *arena, Token token, AST *left)
{
	AST *node = allocate_struct(arena, AST);
	*node = { token, left->consumed + 1, left };
	return(node);
}

AST *make_operator_node(Memory_Arena *arena, Token token, AST *left, AST *right)
{
	AST *node = allocate_struct(arena, AST);
	*node = { token, left->consumed + right->consumed + 1, left, right };
	switch(token.text[0])
	{
		case '+': node->precedence = Precedence::Addition;        break;
		case '-': node->precedence = Precedence::Subtraction;    break;
		case '*': node->precedence = Precedence::Multiplication; break;
		case '/': node->precedence = Precedence::Division;       break;
		case '^': node->precedence = Precedence::Exponentiation; break;
	}
	return(node);
}

AST *parse_prefixed_term(Memory_Arena *arena, Token_List tokens)
{
	AST *term = 0;
	if (is_number_or_variable(tokens[1]))
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
	return(prefixed_term);
}

AST *parse_tokens(Memory_Arena *arena, Token_List tokens)
{
	AST *node = 0;
	if (tokens.count)
	{
		Token token = tokens[0];

		if (is_number_or_variable(token))
			node = make_term_node(arena, token);
		else if (is_open_parenthesis(token))
		{
			node = parse_tokens(arena, tokens_from(tokens, 1));
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
		else if (!is_end_of_expression(token))
		{
			if (!node)
				node = make_node(arena);
			node->invalid = true;
		}
	}
	return(node);
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
	return(result);
}

internal Result
evaluate_tree(AST *tree, Context *context)
{
	Result result = {};
	if (tree)
	{
		Token token = tree->token;
		// @TODO: variables!
		if (token.type == Token_Type::Number)
		{
			result = { true, parse_float(token.text) };
		}
		else if (token.type == Token_Type::Variable)
		{
			result = (*context)[token.text];
		}
		else if (token.type == Token_Type::Operator)
		{
			Result left_result = evaluate_tree(tree->left, context);
			if (left_result.valid)
			{
				Result right_result = evaluate_tree(tree->right, context);
				if (right_result.valid)
				{
					if (token.text[0] == '+')
						result.value = left_result.value + right_result.value;
					else if (token.text[0] == '-')
						result.value = left_result.value - right_result.value;
					else if (token.text[0] == '*')
						result.value = left_result.value * right_result.value;
					else if (token.text[0] == '/')
						result.value = left_result.value / right_result.value;
					else if (token.text[0] == '^')
						result.value = pow(left_result.value, right_result.value);
					result.valid = true;
				}
				else
				{
					if (tree->precedence >= Precedence::Annex)
					{
						if (token.text[0] == '-') 
						{
							result = { true, -left_result.value };
						}
						else if (token.text[0] == '!')
						{
							result = factorial(left_result.value);
						}
					}
				}
			}
		}
	}
	return(result);
}

internal Result
evaluate_expression(Memory_Arena *arena, U32_String expression, Context *context)
{
	Token_List tokens = tokenize_expression(arena, expression);
	AST *tree = parse_tokens(arena, tokens);
	Result result = evaluate_tree(tree, context);
	return(result);
}

void add_or_update_variable(Context *context, U32_String variable, f64 value)
{
	bool32 existed = false;
	for (u64 i = 0; i < context->count; ++i)
	{
		if (strings_are_equal(context->variables[i], variable))
		{
			context->values[i] = value;
			existed = true;
			break;
		}
	}
	if (!existed)
	{
		assert(context->count < context->capacity);
		context->variables[context->count] = variable;
		context->values[context->count] = value;
		++context->count;
	}
}

Context make_context(Memory_Arena *arena, u64 capacity)
{
	Context context = {};
	context.variables = allocate_array(arena, U32_String, capacity);
	context.values    = allocate_array(arena, f64, capacity);
	context.capacity  = capacity;
	return(context);
}