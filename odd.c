#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <fcntl.h>
#include <unistd.h>

typedef struct ODLData ODLData;

typedef struct Object Object;

typedef char * ODLWord;

typedef float ODLNum;

typedef struct ODLList {
	size_t alloc;
	ODLData * bottom;
	ODLData * top;
} ODLList;

typedef Object * ODLObject;

typedef char * ODLString;

typedef uint32_t ODLInt;

typedef enum ODLDataType {
	ODL_ERROR,
	ODL_WORD,
	ODL_NUM,
	ODL_LIST,
	ODL_OBJECT,
	ODL_STRING,
	ODL_INT,
	ODL_BUILTIN,
	ODL_SYMBOL,
} ODLDataType;

typedef struct ODLDictionary ODLDictionary;

typedef void (*ODLBuiltin)(ODLList * stack, ODLDictionary * dictionary);

typedef struct ODLData{
	ODLDataType type;
	union {
		ODLWord word;
		ODLNum num;
		ODLList * list;
		ODLObject object;
		ODLString string;
		ODLInt integer;
		ODLBuiltin builtin;
	} value;
} ODLData;

typedef struct ODLDefStack {
	ODLWord name;
	ODLList code;
} ODLDefStack;

typedef struct ODLDictionary {
	size_t alloc;
	size_t count;
	ODLDefStack * defs;
} ODLDictionary;


typedef struct ODLWordMap{
	size_t alloc;
	size_t count;
	ODLWord * wordList;
} ODLWordMap;

char isNumeric(char text){
	return (text>=48 && text<=57);
}

void pushODL(ODLList * stack, ODLData data){
	if(stack->top-stack->alloc>=stack->bottom){
		int count=stack->top-stack->bottom;
		stack->alloc*=2;
		stack->bottom=realloc(stack->bottom, stack->alloc*sizeof(ODLData));
		stack->top=stack->bottom+count;
	}
	*(stack->top)=data;
	stack->top++;
}

void unshiftODL(ODLList * stack, ODLData data){
	if(stack->top-stack->alloc>=stack->bottom){
		int count=stack->top-stack->bottom;
		stack->alloc*=2;
		stack->bottom=realloc(stack->bottom, stack->alloc*sizeof(ODLData));
		stack->top=stack->bottom+count;
	}
	
	ODLData * top=(stack->top);
	while(top>stack->bottom){
		*top=*(top-1);
		top--;
	}
	*(stack->bottom)=data;
	stack->top++;
}

ODLData shiftODL(ODLList * stack){
		
	ODLData res=*(stack->bottom);
	ODLData * cur=stack->bottom;
	while(cur<stack->top-1){
		*cur=*(cur+1);
		cur++;
	}
	stack->top--;
	
	return res;
}

ODLData popODL(ODLList * stack){
	if(stack->top==stack->bottom){
		printf("Tried to pop from an empty stack");
		exit(1);
	}
	ODLData d=*(stack->top-1);
	stack->top--;
	return d;
}

void reverseODL(ODLList * stack){
	ODLData temp;
	ODLData * bottom=stack->bottom;
	ODLData * top=stack->top-1;
	while(top>bottom){
		temp=*top;
		*top=*bottom;
		*bottom=temp;
		top--;
		bottom++;
	}
}
void dumpODL(ODLList * stack, int indent);

void dumpODLData(ODLData d, int indent){
	
	char tabString[indent+1];
	int i;
	for(i=0; i<indent; i++){
		tabString[i]='\t';
	}
	tabString[i]=0;
	
	if(d.type==ODL_WORD){
		printf("%sWord: %s\n", tabString, d.value.word);
		return;
	}
	if(d.type==ODL_SYMBOL){
		printf("%sSymbol: %s\n", tabString, d.value.word);
		return;
	}
	if(d.type==ODL_NUM){
		printf("%sNum: %f\n", tabString, d.value.num);
		return;
	}
	if(d.type==ODL_INT){
		printf("%sInt: %d\n", tabString, d.value.integer);
		return;
	}
	if(d.type==ODL_LIST){
		dumpODL(d.value.list, indent+1);
		return;
	}
	printf("%sUnknown type: %d\n", tabString, d.type);
}

void dumpODLr(ODLList * stack, int indent, char reverse){
	char tabString[indent+1];
	int i;
	for(i=0; i<indent-1; i++){
		tabString[i]='\t';
	}
	tabString[i]=0;
	printf("%sList: %ld\n", tabString, stack->top-stack->bottom);
	char dir=reverse ? -1 : 1;
	int index=0;
	for(ODLData * it=(reverse ? stack->top-1 : stack->bottom); reverse ? it>=stack->bottom : it!=stack->top; it+=dir){
		printf("%d: ", index++);
		dumpODLData(*it, indent);
	}
}

void dumpODL(ODLList * stack, int indent){
	dumpODLr(stack, indent, 0);
}

ODLList * allocList(size_t size){
	ODLList * stack=malloc(sizeof(ODLList));
	stack->alloc=size > 8 ? size : 8;
	stack->bottom=malloc(stack->alloc*sizeof(ODLData));
	stack->top=stack->bottom;
	return stack;
}

char * findInWordMap(ODLWord word, ODLWordMap * map){
	for(int i=0; i<map->count; i++){
		if(strcmp(map->wordList[i], word)==0){
			return map->wordList[i];
		}
	}
	if(map->count>=map->alloc){
		map->alloc*=2;
		map->wordList=realloc(map->wordList, map->alloc*sizeof(ODLWord));
	}
	
	char * newWord=malloc(strlen(word)+1);
	strncpy(newWord, word, strlen(word)+1);
	map->wordList[map->count++]=newWord;
	return newWord;
}

ODLList * parseODL(char * code, ODLWordMap * map){
	
	ODLList * stack=allocList(1024);

	char * token=strtok(code, " \t\n");
	while(token!=NULL){
		char foundFloat=0;
		ODLData d;
		char first=1;
		int i=0;
		while(isNumeric(token[i]) || (first && token[i]=='-') || (token[i]=='.' && !foundFloat)){
			if(token[i]=='.'){
				foundFloat=1;
			}
			first=0;
			i++;
		}
		if(token[i]==0 && !(i==1 && token[i-1]=='-')){
			if(foundFloat){
				float v;
				if(!sscanf(token, "%f", &v)){
					printf("Float recognition failed");
					exit(1);
				}
				d.type=ODL_NUM;
				d.value.num=v;
			}else{
				int v;
				if(!sscanf(token, "%d", &v)){
					printf("Integer recognition failed");
					exit(1);
				}
				d.type=ODL_INT;
				d.value.integer=v;
			}
		}else{
			d.type=ODL_WORD;
			d.value.word=findInWordMap(token, map);
		}
		pushODL(stack, d);
		token=strtok(NULL, " \t\n");
	}
	
	reverseODL(stack);

	return stack;
}


ODLData findInDictionary(ODLDictionary * dictionary, char * name){
	for(int i=0; i<dictionary->count; i++){
		if(dictionary->defs[i].name==name){
			ODLList * list=&(dictionary->defs[i].code);
			ODLData * entry=list->top-1;
			return *entry;
		}
	}
	printf("Could not find %s in dictionary\n", name);
	exit(1);
}

void pushToDictionary(ODLDictionary * dictionary, ODLWord name, ODLData d){
	for(int i=0; i<dictionary->count; i++){
		if(dictionary->defs[i].name==name){
			pushODL(&(dictionary->defs[i].code), d);
			return;
		}
	}
	
	if(dictionary->count>=dictionary->alloc){
		dictionary->alloc*=2;
		dictionary->defs=realloc(dictionary->defs, dictionary->alloc*sizeof(ODLDefStack));
	}
	
	dictionary->defs[dictionary->count].name=name;
	
	ODLList * newList=&(dictionary->defs[dictionary->count].code);
	newList->alloc=8;
	newList->bottom=malloc(newList->alloc*sizeof(ODLData));
	newList->top=newList->bottom;
	
	dictionary->count++;
	pushODL(newList, d);
}

ODLData copyODL(ODLData d){
	if(d.type==ODL_LIST){
		ODLList * old=d.value.list;
		d.value.list=allocList(old->top-old->bottom);
		for(ODLData * it=old->bottom; it!=old->top; it++){
			pushODL(d.value.list, copyODL(*it));
		}
	}
	return d;
}

void freeODL(ODLData * d);

void popFromDictionary(ODLDictionary * dictionary, ODLWord name){
	for(int i=0; i<dictionary->count; i++){
		if(dictionary->defs[i].name==name){
			ODLData old=popODL(&(dictionary->defs[i].code));
			freeODL(&old);
			return;
		}
	}
}

void freeListODL(ODLList * list){

	while(list->top>list->bottom){
		list->top--;
		ODLData * cur=list->top;
		freeODL(cur);
	}
	free(list->bottom);
	free(list);
}

void freeODL(ODLData * d){
	if(d->type==ODL_LIST){
		freeListODL(d->value.list);
	}
}

void addBuiltin(ODLDictionary * dictionary, ODLWord name, ODLBuiltin builtin, ODLWordMap * map){
	ODLData d;
	d.type=ODL_BUILTIN;
	d.value.builtin=builtin;

	pushToDictionary(dictionary, findInWordMap(name, map), d);
}

void pushStackODL(ODLList * to, ODLList * source){
	while(source->top!=source->bottom){
		ODLData d=popODL(source);
		pushODL(to, d);
	}
}

void unrollODL(ODLList * stack, ODLDictionary * dictionary);

void executeODL(ODLList * stack, ODLDictionary * dictionary){
	while(stack->top>stack->bottom){
		ODLData * cur=stack->top-1;
		
		if(cur->type==ODL_WORD){

			ODLData def=findInDictionary(dictionary, cur->value.word);
			stack->top--;
			def=copyODL(def);
			pushODL(stack, def);
			unrollODL(stack, dictionary);
		}else if(cur->type==ODL_BUILTIN){
			stack->top--;
			cur->value.builtin(stack, dictionary);
		}else{
			return;
		}
	}
}

void unrollODL(ODLList * stack, ODLDictionary * dictionary){

	if(stack->top==stack->bottom){
		return;
	}
	ODLData * cur=stack->top-1;

	if(cur->type==ODL_LIST){
		ODLData d =popODL(stack);

		pushStackODL(stack, d.value.list);
		freeODL(&d);
	}
}

void evalODLB(ODLList * stack, ODLDictionary * dictionary){
	executeODL(stack, dictionary);
	unrollODL(stack, dictionary);
}

void listODLB(ODLList * stack, ODLDictionary * dictionary){

	executeODL(stack, dictionary);

	ODLData top=popODL(stack);

	if(top.type!=ODL_INT){
		printf("1st arg to list was not an integer");
		exit(1);
	}
	int count=top.value.integer;
	ODLList * newList=allocList(count);

	if(count>stack->top-stack->bottom){
		printf("Empty stack in list\n");
		exit(1);
	}

	for(int i=0; i<count; i++){
		ODLData t=popODL(stack);
		pushODL(newList, t);
	}

	ODLData d;
	d.type=ODL_LIST;
	d.value.list=newList;

	pushODL(stack, d);
}

void ifODLB(ODLList * stack, ODLDictionary * dictionary){

	executeODL(stack, dictionary);
	ODLData top=popODL(stack);

	if(top.type!=ODL_INT){
		printf("1st arg to if was not an integer");
		dumpODLData(top, 0);
		exit(1);
	}
	char cond=(top.value.integer!=0);

	executeODL(stack, dictionary);
	ODLData truePath=popODL(stack);

	executeODL(stack, dictionary);

	ODLData falsePath=popODL(stack);

	pushODL(stack, cond ? truePath : falsePath);

	unrollODL(stack, dictionary);
}

void carryODLB(ODLList * stack, ODLDictionary * dictionary){
	
	executeODL(stack, dictionary);
	ODLData d=popODL(stack);
	
	//dumpODLData(d, 0);

	executeODL(stack, dictionary);

	pushODL(stack, d);
	unrollODL(stack, dictionary);
}

void rawDefineODLB(ODLList * stack, ODLDictionary * dictionary){
	executeODL(stack, dictionary);
	ODLData named=popODL(stack);
	if(named.type!=ODL_SYMBOL){
		printf("define's first argument is something other than a symbol\n");
		exit(1);
	}else{
		ODLWord name=named.value.word;
		
	
		executeODL(stack, dictionary);
		ODLData d=popODL(stack);

		pushToDictionary(dictionary, name, d);
	}
	
}

void popDefineODLB(ODLList * stack, ODLDictionary * dictionary){
	executeODL(stack, dictionary);
	ODLData named=popODL(stack);
	if(named.type!=ODL_SYMBOL){
		printf("pop_define's first argument is something other than a symbol\n");
		exit(1);
	}else{
		ODLWord name=named.value.word;
			
		popFromDictionary(dictionary, name);
	}
	
}

void openParsedBracketODLB(ODLList * stack, ODLDictionary * dictionary){
	
	ODLList * newList=allocList(16);
	ODLData * ele=stack->top-1;
	while(1){
		executeODL(stack, dictionary);
		ODLData cur=popODL(stack);
		if(cur.type==ODL_SYMBOL && strcmp(cur.value.word, ")")==0){
			break;
		}
		pushODL(newList, cur);
		ele=stack->top-1;
	}

	ODLData d;
	d.type=ODL_LIST;
	d.value.list=newList;
	pushODL(stack, d);
}

void openBracketODLB(ODLList * stack, ODLDictionary * dictionary){
	
	int depth=1;
	ODLData * cur=stack->top;
	ODLList * newList=allocList(16);
	while(depth>0 && cur>stack->bottom){
		cur=stack->top-1;
		if(cur->type==ODL_WORD){
			if(strcmp(cur->value.word, "(")==0 || strcmp(cur->value.word, "`(")==0){
				depth++;
			} else if(strcmp(cur->value.word, ")")==0){
				depth--;
			} else if(depth==1 && strcmp(cur->value.word, "`")==0){
				
				popODL(stack);

				executeODL(stack, dictionary);

			}
		}
		
		ODLData t=popODL(stack);
		if(depth!=0){
			pushODL(newList, t);
		}
	}
	
	if(depth>0){
		printf("Missing close bracket\n");
		exit(1);
	}

	
	ODLData d;
	d.type=ODL_LIST;
	d.value.list=newList;
	pushODL(stack, d);
	
}


void parseODLB(ODLList * stack, ODLDictionary * dictionary){
	printf("Misplaced backtick\n");
	exit(1);
}

void closeBracketODLB(ODLList * stack, ODLDictionary * dictionary){
	ODLData d;
	d.type=ODL_SYMBOL;
	d.value.word=")";
	pushODL(stack, d);
}

void swapODLB(ODLList * stack, ODLDictionary * dictionary){
	executeODL(stack, dictionary);
	ODLData cur=popODL(stack);
	if(cur.type!=ODL_INT){
		printf("Tried to swap with non-integer");
		exit(1);
	}
	int firstOffset=cur.value.integer+1;

	executeODL(stack, dictionary);
	cur=popODL(stack);
	if(cur.type!=ODL_INT){
		printf("Tried to swap with non-integer");
		exit(1);
	}
	int secondOffset=cur.value.integer+1;

	if(stack->top-firstOffset<stack->bottom || stack->top-secondOffset<stack->bottom){
		printf("Swap operation out of range\n");
		exit(1);
	}

	ODLData temp=*(stack->top-firstOffset);
	*(stack->top-firstOffset)=*(stack->top-secondOffset);
	*(stack->top-secondOffset)=temp;
}

void discardODLB(ODLList * stack, ODLDictionary * dictionary){
	executeODL(stack, dictionary);
	ODLData cur=popODL(stack);
	if(cur.type!=ODL_INT){
		printf("Tried to discard with non-int");
		exit(1);
	}
	for(int i=0; i<cur.value.integer; i++){
		ODLData d=popODL(stack);
		freeODL(&d);
	}
}

void pushODLB(ODLList * stack, ODLDictionary * dictionary){
	executeODL(stack, dictionary);
	ODLData cur=popODL(stack);
	if(cur.type!=ODL_LIST){
		printf("Tried to push with non-list");
		exit(1);
	}

	executeODL(stack, dictionary);
	ODLData item=popODL(stack);

	pushODL(cur.value.list, item);
	
	pushODL(stack, cur);
}

void pushListODLB(ODLList * stack, ODLDictionary * dictionary){
	executeODL(stack, dictionary);
	ODLData cur=popODL(stack);
	if(cur.type!=ODL_LIST){
		printf("Tried to push with non-list");
		exit(1);
	}

	executeODL(stack, dictionary);
	ODLData copied=popODL(stack);
	if(copied.type!=ODL_LIST){
		printf("Tried to push with non-list");
		exit(1);
	}

	ODLList * list=copied.value.list;

	for(ODLData * it=list->bottom; it!=list->top; it++){
		pushODL(cur.value.list, *it);
	}

	list->top=list->bottom;
	freeListODL(list);

	pushODL(stack, cur);
}

void popODLB(ODLList * stack, ODLDictionary * dictionary){
	executeODL(stack, dictionary);
	ODLData * cur=stack->top-1;
	if(cur->type!=ODL_LIST){
		printf("Tried to pop with non-list");
		exit(1);
	}

	ODLData d=popODL(cur->value.list);
	freeODL(&d);
}

void peekODLB(ODLList * stack, ODLDictionary * dictionary){
	executeODL(stack, dictionary);
	ODLData cur=popODL(stack);
	if(cur.type!=ODL_LIST){
		printf("Tried to peek with non-list");
		exit(1);
	}

	ODLData item=popODL(cur.value.list);
	freeODL(&cur);
	pushODL(stack, item);
}

void restODLB(ODLList * stack, ODLDictionary * dictionary){
	executeODL(stack, dictionary);
	ODLData * cur=stack->top-1;
	if(cur->type!=ODL_LIST){
		printf("Tried to rest with non-list");
		exit(1);
	}

	ODLData d=shiftODL(cur->value.list);
	freeODL(&d);

}

void unshiftODLB(ODLList * stack, ODLDictionary * dictionary){
	executeODL(stack, dictionary);
	ODLData cur=popODL(stack);
	if(cur.type!=ODL_LIST){
		printf("Tried to unshift with non-list");
		exit(1);
	}

	executeODL(stack, dictionary);
	ODLData item=popODL(stack);

	unshiftODL(cur.value.list, item);
	
	pushODL(stack, cur);
}

void getODLB(ODLList * stack, ODLDictionary * dictionary){
	executeODL(stack, dictionary);
	ODLData cur=popODL(stack);
	if(cur.type!=ODL_LIST){
		printf("Tried to get with non-list");
		exit(1);
	}

	executeODL(stack, dictionary);
	ODLData index=popODL(stack);
	if(index.type!=ODL_INT){
		printf("Tried to get with non-integer");
		exit(1);
	}

	int i=index.value.integer;
	
	if(i<0){
		printf("Get index out of range");
		exit(1);
	}

	ODLList * list=cur.value.list;
	if(list->top-list->bottom<i){
		printf("Get index out of range");
		exit(1);
	}

	ODLData item=*(list->bottom+i);
	(list->bottom+i)->type=ODL_ERROR;
	freeListODL(list);

	pushODL(stack, item);
}

void lengthODLB(ODLList * stack, ODLDictionary * dictionary){
	executeODL(stack, dictionary);
	ODLData cur=popODL(stack);
	if(cur.type!=ODL_LIST){
		printf("Tried length with non-list");
		exit(1);
	}
	int length=cur.value.list->top-cur.value.list->bottom;
	cur.type=ODL_INT;
	cur.value.integer=length;
	pushODL(stack, cur);
}

void duplicateODLB(ODLList * stack, ODLDictionary * dictionary){
	executeODL(stack, dictionary);
	ODLData cur=popODL(stack);
	if(cur.type!=ODL_INT){
		printf("Tried to duplicate with non-int");
		exit(1);
	}
	int copies=cur.value.integer;
	while(copies--){
		ODLData cur=copyODL(*(stack->top-1));
		pushODL(stack, cur);
	}
}

void copyODLB(ODLList * stack, ODLDictionary * dictionary){
	executeODL(stack, dictionary);
	ODLData cur=popODL(stack);
	if(cur.type!=ODL_INT){
		printf("Tried to copy with non-int");
		exit(1);
	}
	int source=cur.value.integer+1;
	
	cur=popODL(stack);
	if(cur.type!=ODL_INT){
		printf("Tried to copy with non-int");
		exit(1);
	}
	int dest=cur.value.integer+1;
	
	if(stack->top-dest<stack->bottom || stack->top-source<stack->bottom){
		printf("Copy operation out of range\n");
		exit(1);
	}
	
	ODLData * destp=stack->top-dest;
	freeODL(destp);
	*destp=copyODL(*(stack->top-source));

}

void asSymbolODLB(ODLList * stack, ODLDictionary * dictionary){

	ODLData * cur=(stack->top-1);
	if(cur->type==ODL_WORD){
		cur->type=ODL_SYMBOL;
	}
}

void asWordODLB(ODLList * stack, ODLDictionary * dictionary){
	
	executeODL(stack, dictionary);
	ODLData * cur=(stack->top-1);
	if(cur->type!=ODL_SYMBOL){
		printf("as-word with non-symbol");
		exit(1);
	}
	cur->type=ODL_WORD;
}



typedef int (*intArithmeticCB)(int, int);
typedef float (*floatArithmeticCB)(float, float);
typedef int (*floatComparisonCB)(float, float);

void genericArithmeticODLB(ODLList * stack, ODLDictionary * dictionary, intArithmeticCB iCb, floatArithmeticCB fCb){
	executeODL(stack, dictionary);
	ODLData first=popODL(stack);
	if(first.type!=ODL_INT && first.type!=ODL_NUM){
		printf("Tried arithmetic with non-number");
		exit(1);
	}
	
	executeODL(stack, dictionary);
	ODLData second=popODL(stack);
	if(second.type!=ODL_INT && second.type!=ODL_NUM){
		printf("Tried arithmetic with non-number");
		exit(1);
	}

	ODLData d;	
	if(first.type==ODL_INT && second.type==ODL_INT && iCb!=NULL){
		d.type=ODL_INT;
		d.value.integer=iCb(first.value.integer, second.value.integer);
	}else{
		d.type=ODL_NUM;
		float f1=(first.type == ODL_INT ? (float)first.value.integer : first.value.num);
		float f2=(second.type == ODL_INT ? (float)second.value.integer : second.value.num);
		d.value.num=fCb(f1, f2);
	}

	pushODL(stack, d);
}

int intAdd(int a, int b){ return a+b;}
float floatAdd(float a, float b){ return a+b;}

void addODLB(ODLList * stack, ODLDictionary * dictionary){
	genericArithmeticODLB(stack, dictionary, &intAdd, &floatAdd);
}
int intMinus(int a, int b){ return a-b;}
float floatMinus(float a, float b){ return a-b;}

void minusODLB(ODLList * stack, ODLDictionary * dictionary){
	genericArithmeticODLB(stack, dictionary, &intMinus, &floatMinus);
}

int intMultiply(int a, int b){ return a*b;}
float floatMultiply(float a, float b){ return a*b;}

void multiplyODLB(ODLList * stack, ODLDictionary * dictionary){
	genericArithmeticODLB(stack, dictionary, &intMultiply, &floatMultiply);
}

float floatDivide(float a, float b){ return a/b;}

void divideODLB(ODLList * stack, ODLDictionary * dictionary){
	genericArithmeticODLB(stack, dictionary, NULL, &floatDivide);
}

void genericComparisonODLB(ODLList * stack, ODLDictionary * dictionary, intArithmeticCB iCb, floatComparisonCB fCb){
	executeODL(stack, dictionary);
	ODLData first=popODL(stack);
	if(first.type!=ODL_INT && first.type!=ODL_NUM){
		printf("Tried magnitude comparison with non-number");
		exit(1);
	}
	
	executeODL(stack, dictionary);
	ODLData second=popODL(stack);
	if(second.type!=ODL_INT && second.type!=ODL_NUM){
		printf("Tried magnitude comparison with non-number");
		exit(1);
	}

	ODLData d;
	d.type=ODL_INT;	
	if(first.type==ODL_INT && second.type==ODL_INT && iCb!=NULL){
		d.value.integer=iCb(first.value.integer, second.value.integer);
	}else{
		float f1=(first.type == ODL_INT ? (float)first.value.integer : first.value.num);
		float f2=(second.type == ODL_INT ? (float)second.value.integer : second.value.num);
		d.value.integer=fCb(f1, f2);
	}

	pushODL(stack, d);
}


int intLessThan(int a, int b){
	return (a < b);
}

int floatLessThan(float a, float b){
	return (a < b);
}

void lessThanODLB(ODLList * stack, ODLDictionary * dictionary){
	genericComparisonODLB(stack, dictionary, &intLessThan, &floatLessThan);
}

int intLessThanEqual(int a, int b){
	return (a <= b);
}

int floatLessThanEqual(float a, float b){
	return (a <= b);
}

void lessThanEqualODLB(ODLList * stack, ODLDictionary * dictionary){
	genericComparisonODLB(stack, dictionary, &intLessThanEqual, &floatLessThanEqual);
}

int intGreaterThan(int a, int b){
	return (a > b);
}

int floatGreaterThan(float a, float b){
	return (a > b);
}

void greaterThanODLB(ODLList * stack, ODLDictionary * dictionary){
	genericComparisonODLB(stack, dictionary, &intGreaterThan, &floatGreaterThan);
}

int intGreaterThanEqual(int a, int b){
	return (a >= b);
}

int floatGreaterThanEqual(float a, float b){
	return (a >= b);
}

void greaterThanEqualODLB(ODLList * stack, ODLDictionary * dictionary){
	genericComparisonODLB(stack, dictionary, &intGreaterThanEqual, &floatGreaterThanEqual);
}

char checkEquality(ODLData * first, ODLData * second){
	char res;
	if(first->type!=second->type){
		if((first->type==ODL_WORD && second->type==ODL_SYMBOL) || (second->type==ODL_WORD && first->type==ODL_SYMBOL)){
			res=first->value.word==second->value.word;
		}else if(first->type==ODL_INT && second->type==ODL_NUM){
			res=(((float)first->value.integer) == second->value.num);
		}else if(first->type==ODL_NUM && second->type==ODL_INT){
			res=(((float)second->value.integer) == first->value.num);
		}else{
			res=0;
		}
	}else{
		switch(first->type){
			case ODL_LIST:
				printf("List equality nyi\n");
				exit(1);
			break;
			case ODL_INT:
				res=(first->value.integer==second->value.integer);
			break;
			case ODL_NUM:
				res=(first->value.num==second->value.num);
			break;
			case ODL_WORD:
			case ODL_SYMBOL:
				res=first->value.word==second->value.word;
			break;
			case ODL_STRING:
				res=!strcmp(first->value.string, second->value.string);
			break;
			default:
				printf("Other types in equality nyi\n");
				exit(1);
			break;
		}
	}
	return res;
}

void equalODLB(ODLList * stack, ODLDictionary * dictionary){
	executeODL(stack, dictionary);
	ODLData first=popODL(stack);

	executeODL(stack, dictionary);
	ODLData second=popODL(stack);

	char res=checkEquality(&first, &second);

	ODLData d;
	d.type=ODL_INT;
	d.value.integer=res;
	pushODL(stack, d);
}

void genericLogicalODLB(ODLList * stack, ODLDictionary * dictionary, intArithmeticCB iCb){
	executeODL(stack, dictionary);
	ODLData first=popODL(stack);
	if(first.type!=ODL_INT){
		printf("Tried logical operator with non-int");
		exit(1);
	}
	
	executeODL(stack, dictionary);
	ODLData second=popODL(stack);
	if(second.type!=ODL_INT){
		printf("Tried logical operator with non-int");
		exit(1);
	}

	ODLData d;
	d.type=ODL_INT;	
	d.value.integer=iCb(first.value.integer, second.value.integer);

	pushODL(stack, d);
}

int andLog(int a, int b){
	return (a && b);
}

void andODLB(ODLList * stack, ODLDictionary * dictionary){
	genericLogicalODLB(stack, dictionary, &andLog);
}

int orLog(int a, int b){
	return (a || b);
}

void orODLB(ODLList * stack, ODLDictionary * dictionary){
	genericLogicalODLB(stack, dictionary, &orLog);
}

int xorLog(int a, int b){
	return (!a != !b);
}

void xorODLB(ODLList * stack, ODLDictionary * dictionary){
	genericLogicalODLB(stack, dictionary, &xorLog);
}

void dumpODLB(ODLList * stack, ODLDictionary * dictionary){
	printf("Start Dump");
	dumpODLr(stack, 0, 1);
	printf("\n");
}

void initDictionary(ODLDictionary * dictionary, ODLWordMap * map){

	dictionary->count=0;
	dictionary->alloc=1024;
	dictionary->defs=malloc(dictionary->alloc*sizeof(ODLDefStack));

	addBuiltin(dictionary, "carry", &carryODLB, map);
	addBuiltin(dictionary, "eval", &evalODLB, map);
	addBuiltin(dictionary, "list", &listODLB, map);
	addBuiltin(dictionary, "if", &ifODLB, map);
	addBuiltin(dictionary, "define", &rawDefineODLB, map);
	addBuiltin(dictionary, "pop_define", &popDefineODLB, map);
	addBuiltin(dictionary, "(", &openBracketODLB, map);
	addBuiltin(dictionary, "`(", &openParsedBracketODLB, map);
	addBuiltin(dictionary, ")", &closeBracketODLB, map);
	addBuiltin(dictionary, "`", &parseODLB, map);
	addBuiltin(dictionary, "swap", &swapODLB, map);
	addBuiltin(dictionary, "+", &addODLB, map);
	addBuiltin(dictionary, "-", &minusODLB, map);
	addBuiltin(dictionary, "*", &multiplyODLB, map);
	addBuiltin(dictionary, "/", &divideODLB, map);
	addBuiltin(dictionary, "=", &equalODLB, map);
	addBuiltin(dictionary, "<", &lessThanODLB, map);
	addBuiltin(dictionary, "<=", &lessThanEqualODLB, map);
	addBuiltin(dictionary, ">", &greaterThanODLB, map);
	addBuiltin(dictionary, ">=", &greaterThanEqualODLB, map);
	addBuiltin(dictionary, "and", &andODLB, map);
	addBuiltin(dictionary, "or", &orODLB, map);
	addBuiltin(dictionary, "xor", &xorODLB, map);
	addBuiltin(dictionary, "dump", &dumpODLB, map);
	addBuiltin(dictionary, "as_symbol", &asSymbolODLB, map);
	addBuiltin(dictionary, "$", &asSymbolODLB, map);
	addBuiltin(dictionary, "as_word", &asWordODLB, map);
	addBuiltin(dictionary, "@", &asWordODLB, map);
	addBuiltin(dictionary, "push", &pushODLB, map);
	addBuiltin(dictionary, "merge", &pushListODLB, map);
	addBuiltin(dictionary, "pop", &popODLB, map);
	addBuiltin(dictionary, "peek", &peekODLB, map);
	addBuiltin(dictionary, "unshift", &unshiftODLB, map);
	addBuiltin(dictionary, "rest", &restODLB, map);
	addBuiltin(dictionary, "duplicate", &duplicateODLB, map);
	addBuiltin(dictionary, "copy", &copyODLB, map);
	addBuiltin(dictionary, "get", &getODLB, map);
	addBuiltin(dictionary, "length", &lengthODLB, map);
	addBuiltin(dictionary, "discard", &discardODLB, map);
}

void main(){

	FILE* fd=fopen("./lib/std.odd", "r");
	
	fseek(fd, 0, SEEK_END);
	int size=ftell(fd);

	char * lib=malloc(size+1);
	memset(lib, 0, size+1);
	fseek(fd, 0, SEEK_SET);
	if(!fread(lib, size, 1, fd)){
		printf("Could not read stdlib %d\n", size);
		exit(1);
	}
	fclose(fd);
	
	
	ODLWordMap map;
	map.count=0;
	map.alloc=1024;
	map.wordList=malloc(map.alloc*sizeof(ODLWord));

	ODLDictionary * dictionary=malloc(sizeof(ODLDictionary));

	initDictionary(dictionary, &map);
	

	ODLList * stack=parseODL(lib, &map);

	executeODL(stack, dictionary);

	if(stack->top!=stack->bottom){
		printf("Standard library returned output:\n");
	
		while(stack->top!=stack->bottom){
			ODLData d=popODL(stack);
			dumpODLData(d, 0);
	
			executeODL(stack, dictionary);
		}
	}
	
	free(lib);
	freeListODL(stack);

	while(1){
		printf("> ");
		char * buffer=malloc(4096);
		size_t bufsize=4096;
		getline(&buffer,&bufsize,stdin);

		stack=parseODL(buffer, &map);

		executeODL(stack, dictionary);

		while(stack->top!=stack->bottom){
			ODLData d=popODL(stack);
			dumpODLData(d, 0);

			executeODL(stack, dictionary);
		}
		freeListODL(stack);
	}
}
