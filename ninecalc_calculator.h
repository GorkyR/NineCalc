#include "grs.h"
#include "ninecalc_memory.h"
#include "ninecalc_string.h"

enum Token_Type
{
	Invalid,
	Number,
	Operator,
	Parenthesis,
	Identifier,
};

struct Token
{
	Token_Type type;
	String text;
};

struct Token_List
{
	Token *tokens;
	u32 count;
};

struct AST_Node
{
	Token token;
	AST_Node *left;
	AST_Node *right;
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

internal inline bool32 is_numeric(u32 codepoint)
{
	return(codepoint >= '0' && codepoint <= '9');
}
internal inline bool32 is_alphabetic(u32 codepoint)
{
	return((codepoint >= 'A' && codepoint <= 'Z') || (codepoint >= 'a' && codepoint <= 'z'));
}
internal inline bool32 is_legal_number_codepoint(u32 codepoint)
{
	return(is_numeric(codepoint) || codepoint == '.' || codepoint == '_');
}
internal inline bool32 is_legal_identifier_codepoint(u32 codepoint)
{
	return(is_alphabetic(codepoint) || is_numeric(codepoint) || codepoint == '_');
}

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

Token_List tokenize(Memory *arena, String text) {
	Token_List token_list = {};
	consume_whitespace(&text);
	while (text.length)
	{
		Token *token = 0;
		u32 c = text[0];
		if ((c >= '0' && c <= '9') || c == '.')
		{
			token = tokenize_number(arena, &text);
		}
		else if (c == '+' || c == '-' || c == '*' || c == '/')
		{
			token = allocate_struct(arena, Token);
			token->type = Token_Type::Operator;
			token->text = substring(text, 0, 1);
			text = substring(text, 1);
		}
		else if (c == '(' || c == ')')
		{
			token = allocate_struct(arena, Token);
			token->type = Token_Type::Parenthesis;
			token->text = substring(text, 0, 1);
			text = substring(text, 1);
		}
		else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c == '_'))
		{
			token = tokenize_identifier(arena, &text);	
		}
		if (!token_list.count)
			token_list.tokens = token;
		++token_list.count;
		consume_whitespace(&text);
	}
	return(token_list);
}

AST_Node parse(Memory *arena, Token_List token_list)
{
	return(Abstract_Syntax_Tree{});
}