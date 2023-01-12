/* stable.c  interpreter: ver: 01.sep.2016 (c) Sandor Schneider
 * gcc's labled goto for better performance
 *
 * 10.dec.2017 klimas, Refactored and commented
 * 11.dec.2017 klimas, changed from (*ip++)() to goto *ip; (gcc extension)
 * 12.dec.2017 klimas, return and data stack extracted from data segment
 *             to avoid stack overflow (cyclic stack with 256 slots) and
 *             avoid overwrite of any other data if something goes wrong
 * 12.dec.2017 moved code, data, stack and return stack segment from static
 *             into vm()
 * 13.dec.2017 Execute C functions
 */

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <unistd.h>
#include <termios.h>
#include <sys/time.h>

static struct termios oldterm; // for later use onexit()
FILE *be;
// #define NEXT token=code[ip++]; goto *words[token];
#define NEXT goto monitor;
#define PRIM_NEXT instructions++; token=code[ip++]; goto *words[token];

/*
static int prof_last_token;
typedef struct prof_t {
	long long cnt;
	long long sum_usec;
} prof_t;
static prof_t prof_tokens[256];
static void prof(char token) {
	static long long prof_usec;
	struct timeval tv[1];
	gettimeofday(tv, 0);
	if(!prof_usec) prof_usec=1000000LL*tv->tv_sec+tv->tv_usec;
	prof_t *p=prof_tokens+prof_last_token;;
	p->cnt++;
	gettimeofday(tv, 0);
   long long new_usec=1000000LL*tv->tv_sec+tv->tv_usec;
	p->sum_usec+=new_usec-prof_usec;
	prof_usec=new_usec;
	prof_last_token=token;
}
static void prof_out(void) {
	int i;
	for(i=0;i<256;i++) {
		prof_t *p=prof_tokens+i;
		if(p->cnt) printf("%c %lld %lldusec\n", i, p->cnt, p->sum_usec*1000/p->cnt);
	}
}
*/
static int one_key(void) {
	struct termios t;

	if (!isatty(0)) return 0;

	tcgetattr(0, &oldterm);
	tcgetattr(0, &t);
	t.c_iflag &= ~(ICRNL | ICRNL | IXON|IXANY|IXOFF);
	t.c_iflag |= BRKINT;
	t.c_oflag &= ~OPOST;
 	t.c_lflag &= ~(ICANON | ECHO);
	t.c_lflag |= ISIG;
	t.c_cc[4]  = 1;
	tcsetattr(0, 0, &t);
	int c=0;
	read(0, &c, 1);
	tcsetattr(0, 0, &oldterm);
	return c;
}
static void rd_freg(int idx, int value, int cur_reg) {
	if(cur_reg==idx) printf("\e[37m\e[41m");
	else             printf("\e[32m");
	printf("%c\e[0m%12f|", 'a'+idx, *(float*)&value);
}
static void rd_reg(int idx, int value, int cur_reg) {
	if(cur_reg==idx) printf("\e[37m\e[41m");
	else             printf("\e[32m");
	printf("%c\e[0m%12d|", 'a'+idx, value);
}
static void dbg(void) {
}
static void vm(char **argv, int argc) {
	int   data[2000];// data segment , register a-z
	short fentry[256];// startaddress of functions A-Z
	char  code[2000];// code segment
	int   stack[256];// cyclic, index by char type, this avoids overwriting of anything
	int   rstack[256];// cyclic, index by char type, this avoids overwriting of anything
	int ip=0, tos=0;
	int cur_reg;
	int instructions=0, i, k=0;
	char sp=0, rp=0;
	char c;
	char token;

	memset(fentry, 0, sizeof(fentry));
	memset(data, 0, sizeof(data));
	memset(code, 0, sizeof(code));
	memset(stack, 0, sizeof(stack));
	memset(rstack, 0, sizeof(rstack));

	for(i=2;i<argc;i++) data[i-2]=atoi(argv[i]); // set arguments into registers
	fread(code, 1, sizeof(code), be); // read program into memory
	fclose(be);

	void *words[]={
/*         0        1        2         3          4        5         6        7        8         9 */
/*  0*/ &&halt,  &&noop,  &&noop,   &&noop,    &&noop,  &&noop,   &&noop,  &&noop,  &&noop,   &&noop,
/* 10*/ &&noop,  &&noop,  &&noop,   &&noop,    &&noop,  &&noop,   &&noop,  &&noop,  &&noop,   &&noop,
/* 20*/ &&noop,  &&noop,  &&noop,   &&noop,    &&noop,  &&noop,   &&noop,  &&noop,  &&noop,   &&noop,
/* 30*/ &&noop,  &&noop,  &&noop,   &&store,   &&print, &&dup_,   &&swap,  &&modulo,&&and,    &&ext,
/* 40*/ &&if_,   &&noop,  &&mul,    &&add,     &&emit,  &&sub,    &&dot,   &&div_,  &&digit,  &&digit,
/* 50*/ &&digit, &&digit, &&digit,  &&digit,   &&digit, &&digit,  &&digit, &&digit, &&reg_set,&&reg_get,
/* 60*/ &&less,  &&equal, &&greater,&&fetch,   &&over,  &&call,   &&call,  &&call,  &&call,   &&call,
/* 70*/ &&call,  &&call,  &&call,   &&call,    &&call,  &&call,   &&call,  &&call,  &&call,   &&call,
/* 80*/ &&call,  &&call,  &&call,   &&call,    &&call,  &&call,   &&call,  &&call,  &&call,   &&call,
/* 90*/ &&call,  &&while_,&&drop,   &&endwhile,&&key,   &&negate, &&system_,   &&reg,   &&reg,    &&reg,
/*100*/ &&reg,   &&reg,   &&reg,    &&reg,     &&reg,   &&reg,    &&reg,   &&reg,   &&reg,    &&reg,
/*110*/ &&reg,   &&reg,   &&reg,    &&reg,     &&reg,   &&reg,    &&reg,   &&reg,   &&reg,    &&reg,
/*120*/ &&reg,   &&reg,   &&reg,    &&def,     &&or,    &&enddef, &&not,   &&noop,  &&noop,   &&noop,
/*130*/ &&flt,   &&unflt, &&dbg,    &&traceon, &&traceoff,&&version1,
	};
	
	noop:          NEXT // MUST BE THE FIRST ENTRY
	halt:          return;  // 0-Byte
	ext:     /*`*/ i=tos;tos=stack[sp--]; goto *words[i+130];// extended operator, index on stack
	reg:     /*az*/k=1;cur_reg=token-97;NEXT

	reg_set: /*:*/ data[cur_reg]=tos;tos=stack[sp--]; k=0; NEXT // store tos into register
	reg_get: /*;*/ stack[++sp]=tos;tos=data[cur_reg]; k=0; NEXT // store register on tos
	store:   /*!*/ data[data[cur_reg]]=tos; tos=stack[sp--];NEXT // cur_reg is address register. This could be implemented as external store with real pointer
	fetch:   /*?*/ stack[++sp]=tos; tos=data[data[cur_reg]];NEXT // cur_reg is address register. This could be implemented as external fetch with real pointer
	print:   /*"*/ while((c=code[ip++])!='\"')putc(c,stdout);NEXT // print until 2nd "
	dup_:    /*#*/ stack[++sp]=tos;NEXT
	drop:    /*\*/ tos=stack[sp--];NEXT
	dbg: dbg(); NEXT // for C debugging
	if_:     /*(*/ if(!tos) while(code[ip++]!=')');tos=stack[sp--];NEXT // until )
	emit:    /*,*/ printf("%c",tos);tos=stack[sp--];NEXT // print character
	traceon:  NEXT
	traceoff:  NEXT
	version1:
	/* < 60 */  words['<']=&&lessv1;
	/* = 61 */  words['=']=&&equalv1;
	/* > 62 */  words['>']=&&greaterv1;
					NEXT
	system_: { /* ` */
		char buffer[256], *p=buffer, *end=buffer+sizeof(buffer)-1;
		char c;
		while((c=code[ip++])!='`') if(p<end) *p++=c;
		*p=0;
		system(buffer);
	}
	dot:     /*.*/ printf("%d",tos);tos=stack[sp--];NEXT // print number
flt_dot:    /*.*/ 
		i=tos;
		printf("%f",*(float*)&i);
		tos=stack[sp--];
		NEXT // print number as float
	key:     /*^*/ stack[++sp]=tos; tos=one_key();NEXT
	enddef:  /*}*/ ip=rstack[rp--];NEXT
	less:    /*<*/ tos=tos>stack[sp--]?-1:0;NEXT
flt_less:   /*<*/ i=tos; tos=(*(float*)&i)>*(float*)&stack[sp--]?-1:0;NEXT
	equal:   /*=*/ tos=tos==stack[sp--]?-1:0;NEXT
   greater: /*>*/ tos=tos<stack[sp--]?-1:0;NEXT
flt_greater:/*>*/ i=tos; tos=(*(float*)&i)<(*(float*)&stack[sp--])?-1:0;NEXT
lessv1:tos=tos>stack[sp]?-1:0; NEXT      // <
equalv1:tos=tos==stack[sp]?-1:0; NEXT    // =
greaterv1:tos=tos<stack[sp]?-1:0; NEXT   // >
	and:     /*&*/ tos=stack[sp--]&tos;NEXT
	or:      /*|*/ tos=stack[sp--]|tos;NEXT
	not:     /*~*/ tos=~tos;NEXT
	negate:  /*_*/ tos=-tos;NEXT
flt_negate: /*_*/ {i=tos; float f=(*(float*)&i*-1.0); tos=*(int*)&f;NEXT}
	modulo:  /*%*/ tos=stack[sp--]%tos;NEXT
	mul:     /***/ tos=stack[sp--]*tos;NEXT
flt_mul:    /***/ i=tos; *(float*)&i=(*(float*)&stack[sp--])*(*(float*)&i);tos=i;NEXT
	div_:    /*/ */tos=stack[sp--]/tos;NEXT
flt_div:    /*/ */i=tos;*(float*)&i=(*(float*)&stack[sp--])/(*(float*)&i);tos=i;NEXT
	add:  // + or increment register
		if(k) {data[cur_reg]++;k=0;}
		else tos=stack[sp--]+tos;
		NEXT
flt_add:  // + or increment register
		if(k) {data[cur_reg]++;k=0;}
		else {
			i=tos;
			*(float*)&i=(*(float*)&stack[sp--])+(*(float*)&i);
			tos=i;
		}
		NEXT
	sub:  // - sub or decrement register
		if(k) {data[cur_reg]--;k=0;}
		else tos=stack[sp--]-tos;
		NEXT
flt_sub:  // + or increment register
		if(k) {data[cur_reg]--;k=0;}
		else {
			i=tos;
			*(float*)&i=(*(float*)&stack[sp--])-(*(float*)&i);
			tos=i;
		}
		NEXT
	digit:  /* build a number */
		stack[++sp]=tos;
		tos=0;
		do {
			tos=tos*10+token-'0';
		} while((token=code[ip++])&&isdigit(token));
		ip--;
		NEXT
flt_digit:  /* build a number */
		{
			stack[++sp]=tos;
			float val;
			char buffer[32], *p=buffer;
			int has_dot=0;
			tos=0;
			do {
				if(token=='.') has_dot=1;
				*p++=token;
			} while((token=code[ip++])&&(isdigit(token)||(!has_dot&&(token=='.')))) ;
			*p=0;
			if(has_dot) {
				val=strtod(buffer, 0);
				tos=*(int*)&val;
			}
			else        tos=strtol(buffer, 0, 0);

			ip--;
		}
		NEXT
	swap: // $
		i=tos;
		tos=stack[sp];
		stack[sp]=i;
		NEXT
	over: // @
		stack[++sp]=tos;
		tos=stack[sp-1];
		NEXT
	call:  // A..Z Call word
		rstack[++rp]=ip; // push current instruction pointer on return stack
		ip=fentry[token-65]; // jump to word in token
		NEXT
	while_:  // [. top of stack will be droped on end of loop
		rstack[++rp]=ip; // push current instruction pointer, for looping
		if(!tos) while((c=code[ip++]) && (c!=']')) ;
		NEXT
	endwhile:  // ] (code 93)
		if(tos) ip=rstack[rp]; // jump back to [ if top of stack is not zeor
		else rp--; // clean up return and data stack

		tos=stack[sp--]; // drop datastack in any case
		NEXT
	def:  // { define word
		i=code[ip++]-65;
		fentry[(char)i]=ip; // point to code where definition already remains
		while((c=code[ip++]) && (c!='}')); // skip forward to } (end of definition)
		NEXT
flt: /*000*/
		words[46]=&&flt_dot;
		words[60]=&&flt_less;
		words[62]=&&flt_greater;
		words[95]=&&flt_negate;
		words[42]=&&flt_mul;
		words[47]=&&flt_div;
		words[43]=&&flt_add;
		words[45]=&&flt_sub;
		words[48]=&&flt_digit;
		words[49]=&&flt_digit;
		words[50]=&&flt_digit;
		words[51]=&&flt_digit;
		words[52]=&&flt_digit;
		words[53]=&&flt_digit;
		words[54]=&&flt_digit;
		words[55]=&&flt_digit;
		words[56]=&&flt_digit;
		words[57]=&&flt_digit;
		NEXT
unflt:/*001*/
		words[46]=&&dot;
		words[60]=&&less;
		words[62]=&&greater;
		words[95]=&&negate;
		words[42]=&&mul;
		words[47]=&&div_;
		words[43]=&&add;
		words[45]=&&sub;
		words[48]=&&digit;
		words[49]=&&digit;
		words[50]=&&digit;
		words[51]=&&digit;
		words[52]=&&digit;
		words[53]=&&digit;
		words[54]=&&digit;
		words[55]=&&digit;
		words[56]=&&digit;
		words[57]=&&digit;
		NEXT

	monitor: // enter monitor mode
		// Display stack, wait for key
		printf("\e[H\e[2KStack %3d: ", sp+1);
		i=tos;
		printf("tos=%d ", tos);
		for(i=sp;i>=(sp>6?sp-7:1);i--) printf("%4d ", stack[i]);
		printf("\n");
		i=tos;
		printf("\e[2KStack flt: tos=%f ", *(float*)&i);
		for(i=sp;i>=(sp>6?sp-7:1);i--) printf("%f ", *(float*)&stack[i]);
		printf("\n");
		// for(i=sp>6?sp-7:1;i<=sp;i++) printf("%4d ", stack[i]);
		printf("\e[2KReturn%3d: ",rp+1);
		printf("ip=%d ", ip);
		// for(i=rp>7?rp-8:1;i<=rp;i++) printf("%4d ", rstack[i]);
		for(i=rp;i>=(rp>7?rp-8:1);i--) printf("%4d ", rstack[i]);
		printf("\n");
		printf("Reg  0: ");
		for(i=0;i<5;i++) rd_freg(i, data[i], cur_reg); // printf("\e[32m%c\e[0m%4d ", 'a'+i, data[i]);
		printf("\n     5: ");
		for(i=0;i<5;i++) rd_freg(i+5, data[i+5], cur_reg); // printf("\e[32m%c\e[0m%4d ", 'a'+10+i, data[10+i]);
		printf("\n    10: ");
		for(i=0;i<5;i++) rd_reg(i+10, data[i+10], cur_reg); // printf("\e[32m%c\e[0m%4d ", 'a'+10+i, data[10+i]);
		printf("\n    20: ");
		for(i=0;i<5;i++) rd_reg(i+15, data[i+15], cur_reg); // printf("\e[32m%c\e[0m%4d ", 'a'+10+i, data[10+i]);
		printf("\n    20: ");
		for(i=0;i<5;i++) rd_reg(i+20, data[i+20], cur_reg); // printf("\e[32m%c\e[0m%4d ", 'a'+20+i, data[20+i]);
		printf("\n");
		int y;
		printf("Data30: ");
		for(y=0;y<10;y++) {
			for(i=0;i<10;i++) printf("%5d ", data[30+y*10+i]);
			printf("\n");
			if(y<10)printf(" %5d: ", 40+y*10);
		}
		printf("\n");
		printf("        0123456789 123456789 123456789 123456789 123456789\n");
		printf("Code 0: ");
		c=1;
		int last_char=0, in_string=0;
		char *color=""; // "\e[40m\e[37m";
		for(y=0;c && y<10;y++) {
			for(i=0;i<50;i++) {

				if(c=code[y*50+i]) {
					if(c=='"') in_string=!in_string;
					if(!in_string) {
						if(c=='{') color="\e[31m";
						if(c=='}') color="\e[31m";
						if(c=='[') color="\e[34m";
						if(c==']') color="\e[34m";
						// if(c=='}') bg="\e[40m";
						if(c>='a' && c<='z') {
							color="\e[32m";
						}
						if((last_char=='{') && (c>='A' && c<='Z')) {
							color="\e[31m";
						}
						if((last_char!='{') && (c>='A' && c<='Z')) {
							color="\e[33m";
						}
						if(c>='0' && c<='9') {
							color="\e[36m";
						}
					}

					if(y*50+i==ip) color="\e[37m\e[41m";
					printf("%s%c", color, isprint(c)?c:' ');
					if(*color) printf("\e[0m");
					color="";
					last_char=c;
				}
			}
			printf("\n");
			if(c) printf(" %5d: ", (y+1)*50);
		}

		printf("Instructions %d\e[J ", instructions);
		fflush(stdout);
		// usleep(50000);
		fgetc(stdin);
		PRIM_NEXT
}
// ~ bitwise inverse (not)

int main(int argc,char *argv[]){
	be=fopen(argv[1], "rp"); 
	if (!be) {printf("usage stable filename [args]\n"); return 1;}

	vm(argv, argc);
	// prof_out();

	return 0;
}
