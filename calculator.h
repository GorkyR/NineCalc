#include "grs.h"
#include "memory.h"
#include "string.h"

enum Token_Type
{
	Invalid,
	Number,
	Operator,
	Parenthesis,
	Identifier,
	End
};

enum Number_Type { Integer, Real };
enum Operator_Precedence  { Addition, Subtraction, Multiplication, Division, Exponentiation };

struct Token
{
	Token_Type type;
	String text;
	union {
		Number_Type number_type;
		Operator_Precedence  precedence;
	};
};

void consume_whitespace(String *text)
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

internal inline bool32 is_numeric(u32 codepoint) { return(codepoint >= '0' && codepoint <= '9'); }
internal inline bool32 is_alphabetic(u32 codepoint) { return((codepoint >= 'A' && codepoint <= 'Z') || (codepoint >= 'a' && codepoint <= 'z')); }
internal inline bool32 is_legal_number_codepoint(u32 codepoint) { return(is_numeric(codepoint) || codepoint == '.' || codepoint == '_'); }
internal inline bool32 is_legal_identifier_codepoint(u32 codepoint) { return(is_alphabetic(codepoint) || is_numeric(codepoint) || codepoint == '_'); }

Token *tokenize_number(Memory *arena, String *text)
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
		token->number_type = decimal? Number_Type::Real : Number_Type::Integer;
	}
	token->text = substring(*text, 0, offset);
	*text = substring(*text, offset);
	return(token);
}

Token *tokenize_identifier(Memory *arena, String *text)
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

struct Token_List
{
	Token *tokens;
	u32 count;

	Token &operator[](u64 index);
};

Token &Token_List::operator[](u64 index)
{
#if 0
	assert(index < this->count);
#else
	index = (index >= this->count? this->count - 1 : index);
#endif
	return(this->tokens[index]);
}

Token_List tokenize(Memory *arena, String text) {
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

struct AST
{
	Token token;
	u32 consumed_tokens;
	AST *left;
	AST *right;
};

// "123 + 456" ->
// num(123), op(+), num(456) ->
// node(+, node(123), node(456))

// "1 + 23 * 456" ->   +
//                    / \
//                   1   *
//                      / \
//                    23   456

// "(1 + 23) * 456" ->
// paren((), num(1), op(+), num(23), paren()), op(*), num(456) ->
// node(*, node(+, node(1), node(23)), node(456))

// "((1 + 23) * 456) / 7890" ->
// paren((), paren((), num(1), op(+), num(23), paren()), op(*), num(456), paren()), op(/), num(7890) ->
// node(/, node(*, node(+, node(1), node(23)), node(456)), node(7890))

// "prev + 5" ->
// ident(prev), op(+), num(5) ->
// node(+, node(prev), node(5))

//                        *                +    
//                       / \              / \   
// "1 * 20 + 300"  =>  10   +     =>     *   30 
//                         / \          / \     
//                       20   30      10   20   

//                        -               +   
//                       / \             / \  
// "500 - 40 + 1"  =>  500  +    =>     -   1 
//                         / \         / \    
//                       40   1      500  40  

//                     ÷            *   
//                    / \          / \  
// "15 / 3 * 4"  =>  15  *   =>   ÷   4 
//                      / \      / \    
//                     3   4    15  3   

AST *parse(Memory *arena, Token_List tokens, u32 from = 0, bool32 in_parenthesis = false)
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
	else
	{
		node = allocate_struct(arena, AST);
		*node = {};
	}

	return(node);
}

s64 parse_integer(String text)
{
	assert(text[0] != '-');    // assume always positive
	assert(text.length <= 18); // max 18-digit integer (1 quintillion - 1)

	s64 result = 0;
	for (u64 i = 0; i < text.length; i++)
	{
		result *= 10;
		u32 codepoint = text[i];
		if (is_numeric(codepoint))
			result += codepoint - '0';
	}

	return(result);
}

f64 parse_real(String text)
{
	assert(text[0] != '-');
	u64 decimal_offset = 0;
	for (; decimal_offset < text.length; ++decimal_offset)
		if (text[decimal_offset] == '.')
			break;

	f64 result = (f64)parse_integer(substring(text, 0, decimal_offset));

	f64 pow_ten = 10;
	for (u64 i = decimal_offset + 2; i < text.length; ++i)
		pow_ten *= 10;
	result += parse_integer(substring(text, decimal_offset + 1)) / pow_ten;

	return(result);
}

struct Result
{
	bool32 valid;
	Number_Type type;
	union { s64 integer; f64 real; } value;
};

Result evaluate(AST node)
{
	Result result = {};
	if (node.token.type == Token_Type::Number)
	{
		result.type = node.token.number_type;
		if (node.token.number_type == Number_Type::Integer)
		{
			result.value.integer = parse_integer(node.token.text);
			result.valid = true;
		}
		else if (node.token.number_type == Number_Type::Real)
		{
			result.value.real = parse_real(node.token.text);
			result.valid = true;
		}
	}
	else if (node.token.type == Token_Type::Operator)
	{
		if (node.token.text[0] == '+')
		{
			Result res1 = evaluate(*node.left);
			Result res2 = evaluate(*node.right);
			if (res1.valid & res2.valid)
			{
				if (res1.type == Number_Type::Real || res2.type == Number_Type::Real)
				{
					result.type = Number_Type::Real;

					if (res1.type == Number_Type::Real)
						result.value.real = res1.value.real;
					else
						result.value.real = (f64)res1.value.integer;

					if (res2.type == Number_Type::Real)
						result.value.real += res2.value.real;
					else
						result.value.real += res2.value.integer;
				}
				else
				{
					result.type = Number_Type::Integer;
					result.value.integer = res1.value.integer + res2.value.integer;
				}
				result.valid = true;
			}
		}

		if (node.token.text[0] == '-')
		{
			Result res1 = evaluate(*node.left);
			Result res2 = evaluate(*node.right);
			if (res1.valid & res2.valid)
			{
				if (res1.type == Number_Type::Real || res2.type == Number_Type::Real)
				{
					result.type = Number_Type::Real;

					if (res1.type == Number_Type::Real)
						result.value.real = res1.value.real;
					else
						result.value.real = (f64)res1.value.integer;

					if (res2.type == Number_Type::Real)
						result.value.real -= res2.value.real;
					else
						result.value.real -= res2.value.integer;
				}
				else
				{
					result.type = Number_Type::Integer;
					result.value.integer = res1.value.integer - res2.value.integer;
				}
				result.valid = true;
			}
		}

		if (node.token.text[0] == '*')
		{
			Result res1 = evaluate(*node.left);
			Result res2 = evaluate(*node.right);
			if (res1.valid & res2.valid)
			{
				if (res1.type == Number_Type::Real || res2.type == Number_Type::Real)
				{
					result.type = Number_Type::Real;

					if (res1.type == Number_Type::Real)
						result.value.real = res1.value.real;
					else
						result.value.real = (f64)res1.value.integer;

					if (res2.type == Number_Type::Real)
						result.value.real *= res2.value.real;
					else
						result.value.real *= res2.value.integer;
				}
				else
				{
					result.type = Number_Type::Integer;
					result.value.integer = res1.value.integer * res2.value.integer;
				}
				result.valid = true;
			}
		}

		if (node.token.text[0] == '/')
		{
			Result res1 = evaluate(*node.left);
			Result res2 = evaluate(*node.right);
			if (res1.valid & res2.valid)
			{
				result.type = Number_Type::Real;
				if (res1.type == Number_Type::Real)
					result.value.real = res1.value.real;
				else
					result.value.real = (f64)res1.value.integer;

				if (res2.type == Number_Type::Real)
					result.value.real /= res2.value.real;
				else
					result.value.real /= res2.value.integer;
				result.valid = true;
			}
		}
	}
	return(result);
}

Result evaluate(Memory *arena, String text)
{
	Token_List tokens = tokenize(arena, text);
	AST *node = parse(arena, tokens);
	return(evaluate(*node));
}