#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "util/gild_basic.h"
#include "util/gild_log.h"
#include "util/gild_mem.h"
#include "dialogue.h"

static bool is_identifier(char c){return isalnum((unsigned char)c) || c == '_';}
static bool is_keybound(struct Lexer *lex, s32 offset){
	s32 idx = gild_safe_add(lex->pos, offset);
	if(idx < 0) return false;
	return !is_identifier(lex->src[idx]);
}
static struct Token *peek(struct Parser *p){return &p->tokens[p->pos];}
static struct Token *consume(struct Parser *p){return &p->tokens[p->pos++];}
static bool match(struct Parser *p, enum TokenType type){
	if(p->tokens[p->pos].type == type){p->pos++; return true;}
	return false;
}
static void parse_entry(struct Parser *p, struct DialogueFile *file);
static void lexer_scan(struct Lexer *lex);
static void parser_file(struct Parser *p, struct DialogueFile *file);

struct DialogueFile *load_dialogue(const char *path){
	if(!path){GILD_LOG(GILD_IS_NULL, "Dialogue path is NULL"); return NULL;}
	FILE *f = fopen(path, "rb");
	if(!f){GILD_LOG(GILD_PARSE, "Failed to open %s", path); return NULL;}

	fseek(f, 0, SEEK_END);
	long raw_size = ftell(f);
	rewind(f);
	if(raw_size < 0){
		GILD_LOG(GILD_READ, "ftell failed for %s", path);
		fclose(f);
		return NULL;
	}

	size_t file_size = gild_safe_cast(size_t, (s64)raw_size);
	size_t alloc_size = gild_safe_add(file_size, (size_t)1);
	char *src = GILD_XCALLOC(1, alloc_size);
	if(!src){fclose(f); return NULL;}

	size_t read_count = fread(src, 1, file_size, f);
	src[read_count] = '\0';
	fclose(f);

	// Lexer
	struct Lexer lex = {0};
	lex.src = src;
	lexer_scan(&lex);

	struct DialogueFile *dialogue = GILD_XCALLOC(1, sizeof(struct DialogueFile));
	if(!dialogue){free(src); return NULL;}

	struct Parser parser = {0};
	parser.tokens = lex.tokens;
	parser.count = lex.count;
	parser_file(&parser, dialogue);
	free(src);
	GILD_LOG(GILD_LOAD, "Loaded dialogue at %s with %d entries", path, dialogue->entry_count);
	return dialogue;
}

static void parser_file(struct Parser *p, struct DialogueFile *file){
	while(peek(p)->type != TMARK_EOF){
		if(peek(p)->type == TBEGIN || peek(p)->type == TIF){
			parse_entry(p, file);
		} else{
			p->pos++;
		}
	}
}

static void parse_entry(struct Parser *p, struct DialogueFile *file){
	if(file->entry_count >= MAX_ENTRIES){
		GILD_LOG(GILD_PARSE, "Too many entries (max %d)", MAX_ENTRIES);
		return;
	}

	struct DialogueEntry *entry = &file->entries[file->entry_count++];
	memset(entry, 0, sizeof(struct DialogueEntry));

	// Optional IF condition before begin [IF ~condition~]
	if(match(p, TIF)){
		struct Token *condition = consume(p);
		if(condition->type == TSTRING){
			strncpy(entry->condition, condition->value, MAX_CONDITION_LENGTH - 1);
			entry->condition[MAX_CONDITION_LENGTH - 1] = '\0';
		}
	}
	if(!match(p, TBEGIN)){
		GILD_LOG(GILD_PARSE, "Expected BEGIN, returning");
		return;
	}

	// BEGIN token
	struct Token *name = consume(p);
	if(name->type == TIDENTIFIER){
		strncpy(entry->name, name->value, MAX_ENTRY_NAME - 1);
		entry->name[MAX_ENTRY_NAME - 1] = '\0';
	} else{
		GILD_LOG(GILD_PARSE, "Expected entry name after BEGIN, returning");
		return;
	}

	// Loop to END token or EOF
	while(peek(p)->type != TEND && peek(p)->type != TMARK_EOF){
		if(entry->node_count >= MAX_NODES_PER_ENTRY){
			GILD_LOG(GILD_PARSE, "%s exceeded node limit with %d (limit %d)",
			         entry->name, entry->node_count, MAX_NODES_PER_ENTRY);
			p->pos++;
			continue;
		}

		// SAY
		if(match(p, TSAY)){
			struct DialogueNode *node = &entry->nodes[entry->node_count++];
			memset(node, 0, sizeof(struct DialogueNode));
			node->type = NODE_SAY;
			if(match(p, TNARRATOR)){node->is_narrator = true;}
			struct Token *text = consume(p);
			if(text->type == TSTRING){
				strncpy(node->text, text->value, MAX_TEXT_LENGTH - 1);
				node->text[MAX_TEXT_LENGTH - 1] = '\0';
			}
			continue;
		}
		if(match(p, TIF)){
			struct Token *condition = consume(p);
			if(peek(p)->type == TREPLY){
				consume(p);
				struct DialogueNode *node = &entry->nodes[entry->node_count++];
				memset(node, 0, sizeof(struct DialogueNode));
				node->type = NODE_REPLY;
				if(condition->type == TSTRING){
					strncpy(node->condition, condition->value, MAX_CONDITION_LENGTH - 1);
					node->condition[MAX_CONDITION_LENGTH - 1] = '\0';
				}
				struct Token *text = consume(p);
				if(text->type == TSTRING){
					strncpy(node->text, text->value, MAX_TEXT_LENGTH - 1);
					node->text[MAX_TEXT_LENGTH - 1] = '\0';
				}
				if(match(p, TGOTO)){
					struct Token *target = consume(p);
					if(target->type == TIDENTIFIER){
						strncpy(node->goto_entry, target->value, MAX_ENTRY_NAME - 1);
						node->goto_entry[MAX_ENTRY_NAME - 1] = '\0';
					}
				} else if(match(p, TEXIT)){
					node->is_exit = true;
				}
			}
			continue;
		}
		if(match(p, TREPLY)){
			struct DialogueNode *node = &entry->nodes[entry->node_count++];
			memset(node, 0, sizeof(struct DialogueNode));
			node->type = NODE_REPLY;
			struct Token *text = consume(p);
			if(text->type == TSTRING){
				strncpy(node->text, text->value, MAX_TEXT_LENGTH - 1);
				node->text[MAX_TEXT_LENGTH - 1] = '\0';
			}
			if(match(p, TGOTO)){
				struct Token *target = consume(p);
				if(target->type == TIDENTIFIER){
					strncpy(node->goto_entry, target->value, MAX_ENTRY_NAME - 1);
					node->goto_entry[MAX_ENTRY_NAME - 1] = '\0';
				}
			} else if(match(p, TEXIT)){
				node->is_exit = true;
			}
		}
		if(match(p, TSET)){
			struct DialogueNode *node = &entry->nodes[entry->node_count++];
			memset(node, 0, sizeof(struct DialogueNode));
			node->type = NODE_SET_FLAG;

			struct Token *flag_name = consume(p);
			if(flag_name->type == TIDENTIFIER){
				strncpy(node->set_flag, flag_name->value, MAX_ENTRY_NAME - 1);
				node->set_flag[MAX_ENTRY_NAME - 1] = '\0';
			}
			struct Token *flag_val = consume(p);
			node->set_flag_value = (strcmp(flag_val->value, "true") == 0);
			continue;
		}
		p->pos++; // unknown token
	}
	match(p, TEND);
}

static void lexer_scan(struct Lexer *lex){
	while(lex->src[lex->pos] != '\0'){
		while(lex->src[lex->pos] == ' ' || lex->src[lex->pos] == '\n' ||
		      lex->src[lex->pos] == '\r' || lex->src[lex->pos] == '\t'){
			lex->pos++;
		}
		if(lex->src[lex->pos] == '\0') break;

		// skip comment
		if(lex->src[lex->pos] == '/' && lex->src[lex->pos + 1] == '/'){
			while(lex->src[lex->pos] != '\n' && lex->src[lex->pos] != '\0'){
				lex->pos++;
			}
			continue;
		}

		if(lex->src[lex->pos] == '~'){
			lex->pos++;
			s32 start = lex->pos;
			while(lex->src[lex->pos] != '~' && lex->src[lex->pos] != '\0'){
				lex->pos++;
			}
			s32 len = gild_safe_sub(lex->pos, start);
			if(len >= MAX_TOKEN_VALUE){len = MAX_TOKEN_VALUE - 1;}

			struct Token t = {0};
			t.type = TSTRING;
			strncpy(t.value, lex->src + start, gild_safe_cast(size_t, len));
			t.value[len] = '\0';

			if(lex->count < MAX_TOKENS){lex->tokens[lex->count++] = t;}
			if(lex->src[lex->pos] == '~'){lex->pos++;}
			else if(lex->src[lex->pos] == '\0'){break;}
			continue;
		}

		for(s32 i = 0; i < TOKEN_COUNT; i++){
			if(i == TMARK_EOF || i == TSTRING || i == TIDENTIFIER){continue;}
			const char *str = NULL;
			enum TokenType tok = TMARK_EOF;
			switch(i){
				#define X(id) case id: str = #id; tok = id; break;
				TOKEN_LIST
				#undef X
				default: str = NULL;
			}
			if(str){
				// Skip the 'T' prefix from macro name (e.g. "TIF" -> "IF")
				if(str[0] == 'T') str++;

				s32 len = gild_safe_cast(s32, strlen(str));
				if(len > 0 && strncmp(lex->src + lex->pos, str, (size_t)len) == 0 &&
				   is_keybound(lex, len)){
					if(lex->count < MAX_TOKENS){
						lex->tokens[lex->count++] = (struct Token){.type = tok};
					}
					lex->pos = gild_safe_add(lex->pos, len);
					break;
				}
			}
		}

		if(isalpha((unsigned char)lex->src[lex->pos]) || lex->src[lex->pos] == '_'){
			s32 start = lex->pos;
			while(is_identifier(lex->src[lex->pos])){lex->pos++;}
			s32 len = gild_safe_sub(lex->pos, start);
			if(len >= MAX_TOKEN_VALUE){len = MAX_TOKEN_VALUE - 1;}

			struct Token t = {0};
			t.type = TIDENTIFIER;
			strncpy(t.value, lex->src + start, gild_safe_cast(size_t, len));
			t.value[len] = '\0';
			if(lex->count < MAX_TOKENS){lex->tokens[lex->count++] = t;}
			continue;
		}
		lex->pos++;
	}
	if(lex->count < MAX_TOKENS){lex->tokens[lex->count++] = (struct Token){TMARK_EOF};}
}
