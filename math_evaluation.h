#pragma once
#include "grs.h"
#include "memory_arena.h"
#include "u32_string.h"

#include <cmath>

enum class Token_Type
{
	Invalid,
	Number,
	Operator,
	Parenthesis,
	Identifier,
	End
};

enum class Operator_Precedence
{
	Invalid,
	Addition,
	Subtraction,
	Multiplication,
	Division,
	Exponentiation
};

struct Token
{
	Token_Type type;
	U32_String text;
	union 
	{
		Operator_Precedence precedence;
	};
};

struct Token_List
{
	Token *tokens;
	u32 count;

	Token &operator[](u64 index);
};

struct AST
{
	Token token;
	u32 consumed_tokens;
	AST *left;
	AST *right;
};

struct Result
{
	bool32 is_valid;
	f64 value;
};

Token_List tokenize(Memory_Arena *arena, U32_String text);
AST *parse(Memory_Arena *arena, Token_List tokens, u32, bool32);
Result evaluate(AST node);
Result evaluate(Memory_Arena *arena, U32_String expression);

/////////////////////////////

Token &Token_List::operator[](u64 index)
{
#if 0
	assert(index < this->count);
#else
	index = (index >= this->count? this->count - 1 : index);
#endif
	return(this->tokens[index]);
}

internal void
consume_whitespace(U32_String *text)
{
	u32 offset = 0;
	while (offset < text->length)
	{
		u32 codepoint = (*text)[offset];
		if (codepoint == ' ' || codepoint == '\t' || codepoint == '\n')
			offset++;
		else
			break;
	}
	*text = substring(*text, offset);
}

internal inline bool32
is_numeric(u32 codepoint)
{
	return(codepoint >= '0' && codepoint <= '9');
}

internal inline bool32
is_alphabetic(u32 codepoint)
{
	return((codepoint >= 'A' && codepoint <= 'Z') || (codepoint >= 'a' && codepoint <= 'z'));
}

internal inline bool32
is_legal_number_codepoint(u32 codepoint)
{
	return(is_numeric(codepoint) || codepoint == '.' || codepoint == '_');
}

internal inline bool32
is_legal_identifier_codepoint(u32 codepoint)
{
	return(is_alphabetic(codepoint) || is_numeric(codepoint) || codepoint == '_');
}

internal Token *
tokenize_number(Memory_Arena *arena, U32_String *text)
{
	Token *token = allocate_struct(arena, Token);
	u32 offset = 0;
	bool32 decimal = false;
	bool32 invalid = false;
	while (offset < text->length)
	{
		u32 codepoint = (*text)[offset];
		if (codepoint == '.')
		{
			if (!decimal)
			{
				decimal = true;
			}
			else
			{
				invalid = true;
				break;
			}
		}
		else if (is_alphabetic(codepoint))
		{
			invalid = true;
		}
		else if (!is_legal_number_codepoint(codepoint))
		{
			break;
		}
		++offset;
	}
	if ((*text)[offset - 1] == '.')
		invalid = true;
	if (invalid)
	{
		token->type = Token_Type::Invalid;
		while ((*text)[offset] != ' ' && offset < text->length)
			offset++;	
	}
	else
	{
		token->type = Token_Type::Number;
	}
	token->text = substring(*text, 0, offset);
	*text = substring(*text, offset);
	return(token);
}

internal Token *
tokenize_identifier(Memory_Arena *arena, U32_String *text)
{
	Token *token = allocate_struct(arena, Token);
	u32 offset = 0;
	while (offset < text->length)
	{
		if (!is_legal_identifier_codepoint((*text)[offset]))
			break;
		offset++;
	}
	token->type = Token_Type::Identifier;
	token->text = substring(*text, 0, offset);
	*text = substring(*text, offset);
	return(token);
}

internal Token *
tokenize_invalid(Memory_Arena *arena, U32_String *text)
{
	Token *token = allocate_struct(arena, Token);
	token->type = Token_Type::Invalid;

	u32 offset = 0;
	while (offset < text->length)
	{
		if (text->data[offset] == ' ')
			break;
		++offset;
	}
	token->text = substring(*text, 0, offset);

	*text = substring(*text, offset);
	return(token);
}

Token_List
tokenize(Memory_Arena *arena, U32_String text)
{
	Token_List token_list = {};
	consume_whitespace(&text);
	while (text.length)
	{
		Token *token = 0;
		u32 c = text[0];
		if (is_numeric(c) || c == '.')
		{
			token = tokenize_number(arena, &text);
		}
		else if (c == '+' || c == '-' || c == '*' || c == '/' || c == '^')
		{
			token = allocate_struct(arena, Token);
			token->type = Token_Type::Operator;
			token->text = substring(text, 0, 1);
			text = substring(text, 1);
			switch(c) {
				case '-': { token->precedence = Operator_Precedence::Subtraction   ; } break;
				case '+': { token->precedence = Operator_Precedence::Addition      ; } break;
				case '/': { token->precedence = Operator_Precedence::Division      ; } break;
				case '*': { token->precedence = Operator_Precedence::Multiplication; } break;
				case '^': { token->precedence = Operator_Precedence::Exponentiation; } break;
			}
		}
		else if (c == '(' || c == ')')
		{
			token = allocate_struct(arena, Token);
			token->type = Token_Type::Parenthesis;
			token->text = substring(text, 0, 1);
			text = substring(text, 1);
		}
		else if (is_alphabetic(c) || (c == '_'))
		{
			token = tokenize_identifier(arena, &text);	
		}
		else
		{
			token = tokenize_invalid(arena, &text);
		}
		if (!token_list.count)
			token_list.tokens = token;
		++token_list.count;
		consume_whitespace(&text);
	}
	Token *token = allocate_struct(arena, Token);
	token->type = Token_Type::End;
	if (!token_list.count)
		token_list.tokens = token;
	++token_list.count;
	return(token_list);
}

AST *
parse(Memory_Arena *arena, Token_List tokens, u32 from = 0, bool32 in_parenthesis = false)
{
	AST *node = 0;

	Token token = tokens[from];
	if (token.type == Token_Type::Number || token.type == Token_Type::Identifier)
	{			
		Token next_token = tokens[from + 1];
		if (next_token.type == Token_Type::Operator)
		{
			node = allocate_struct(arena, AST);
			*node = {};

			node->token = next_token;

			{
				AST *left_node = allocate_struct(arena, AST);
				*left_node = {};
				left_node->token = token;
				left_node->consumed_tokens = 1;

				node->left = left_node;
			}

			node->right = parse(arena, tokens, from + 2, in_parenthesis);
			node->consumed_tokens = 1 + node->left->consumed_tokens + node->right->consumed_tokens;

			if (node->right->token.type == Token_Type::Operator &&
				node->right->token.precedence <= node->token.precedence)
			{
				AST *node_right_left = node->right->left;
				node->right->left = node;
				node = node->right;
				node->left->right = node_right_left;
			}
		}
		else if (next_token.type == Token_Type::Parenthesis)
		{
			if (in_parenthesis && next_token.text[0] == ')')
			{
				node = allocate_struct(arena, AST);
				*node = {};

				node->token = token;
				node->consumed_tokens = 1;
			}
			else 
			{
				node = allocate_struct(arena, AST);
				*node = {};
			}
		}
		else
		{
			node = allocate_struct(arena, AST);
			*node = {};

			node->token = token;
			node->consumed_tokens = 1;
		}
	}
	else if (token.type == Token_Type::Parenthesis)
	{
		if (token.text[0] == '(')
		{
			AST *parenthetical_node = parse(arena, tokens, from + 1, true);
			from += parenthetical_node->consumed_tokens + 1;

			Token next_token = tokens[from + 1];
			if (next_token.type == Token_Type::Operator)
			{
				node = allocate_struct(arena, AST);
				*node = {};

				node->token = next_token;
				node->left  = parenthetical_node;
				node->right = parse(arena, tokens, from + 2, in_parenthesis);
				node->consumed_tokens = 1 + node->left->consumed_tokens + 2 + node->right->consumed_tokens;
			}
			else
			{
				node = parenthetical_node;
				node->consumed_tokens += 2;
			}
		}
		else
		{
			node = allocate_struct(arena, AST);
			*node = {};
		}
	}
	else if (token.type == Token_Type::Operator)
	{
		// @TODO: Unary operators
		if (token.text[0] == '-')
		{
			// @TODO: Unary negation
			node = allocate_struct(arena, AST);
			*node = {};
		}
	}
	else
	{
		node = allocate_struct(arena, AST);
		*node = {};
	}

	return(node);
}

Result
evaluate(AST node)
{
	Result result = {};
	if (node.token.type == Token_Type::Number)
	{
		result.is_valid = true;
		result.value = parse_float(node.token.text);
	}
	else if (node.token.type == Token_Type::Operator)
	{
		Result res1 = evaluate(*node.left);
		Result res2 = evaluate(*node.right);

		if (res1.is_valid)
		{
			if (res2.is_valid)
			{
				if (node.token.text[0] == '+')
					result.value = res1.value + res2.value;
				else if (node.token.text[0] == '-')
					result.value = res1.value - res2.value;
				else if (node.token.text[0] == '*')
					result.value = res1.value * res2.value;
				else if (node.token.text[0] == '/')
					result.value = res1.value / res2.value;
				else if (node.token.text[0] == '^')
					result.value = pow(res1.value, res2.value);
			
				result.is_valid = true;
			}
			else
			{
				if (node.token.text[0] == '-')
				{
					result = res1;
					result.value = -result.value;
				}
			}
		}
	}
	return(result);
}

Result
evaluate(Memory_Arena *arena, U32_String text)
{
	Token_List tokens = tokenize(arena, text);
	AST *node = parse(arena, tokens);
	return(evaluate(*node));
}