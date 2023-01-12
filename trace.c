/* stable.c  interpreter:
 *
 * Idea and_ implementation    Sandor Schneider
 * Refactoring and extended   Andreas Klimas   klimas@w3group.de
 *
 * Tracing and Floating point as extension (see stable_glossar.html)
 * IDE, source and code blocks
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/stat.h>

extern void (*words[])(void);
static FILE *trace_fp;

static int cur_reg,ip;  // selected register, instruction pointer
static int data[4000];// data segment (register and memory)
static char sp=-1,rp=-1,token,k,code[4096];// data stack pointer, return stack pointer,  current executed token, and code segment
static int stack[256]; // data stack
static int rstack[256]; // return stack
static short fentry[256];// startaddress of functions A-Z
static int is_trace, is_flt; // trace mode, floating point mode
static int cur_code_block; // current code block;
static int cur_data_blk; // current data block
static FILE *data_fp; // database file, permanent open

static struct termios oldterm; // for later use onexit()
static jmp_buf do_halt;
static sigjmp_buf do_sigint;

static void trace(void) ;

static void noop(void){}                                   // 01..32
static void grow(int delta) {
	int empty[4000];
	memset(empty, 0, sizeof(empty));
	fseek(data_fp, 0, SEEK_END);
	int i;
	for(i=0;i<delta;i++) {
		fwrite(empty, 1, sizeof(empty), data_fp);
	}
}
static void select_block(int blknr) {
	cur_data_blk=blknr;
	struct stat sb;
	fstat(fileno(data_fp), &sb);
	unsigned blks=sb.st_size/4000;
	if((cur_data_blk+1)>blks) grow(blks-cur_data_blk+10);

	fseek(data_fp, cur_data_blk*sizeof(int)*1000, SEEK_SET);
	fread(data+1000, sizeof(int), 1000, data_fp);
}
static void blocknr(void) {stack[++sp]=cur_data_blk;}
static void save_block(void) {
	tcsetattr(0, 0, &oldterm);
	fseek(data_fp, cur_data_blk*sizeof(int)*1000, SEEK_SET);
	fwrite(data+1000, sizeof(int), 1000, data_fp);
}
static void block(void) {
	save_block(); // write back current block
	select_block(stack[sp--]);
}
static int one_key(void) {
	struct termios t;

	fflush(stdout);
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
static void halt(void) {longjmp(do_halt,1);}               // 0-Byte
static void store(void){data[data[cur_reg]]=stack[sp];sp--;}// !
static void print(void){char c; while((c=code[ip++])!='\"') putc(c,stdout);} // "
static void character(void){stack[++sp]=code[ip++];}
static void system_(void){ // ` system
	char buffer[256], *p=buffer, *end=buffer+sizeof(buffer)-1;
	char c;
	while((c=code[ip++])!='`') if(p<end) *p++=c;
	*p=0;
	system(buffer);
}
static void mstime(void) {
	struct timeval tv[1];
	gettimeofday(tv, 0);
   stack[++sp]=1000*tv->tv_sec+tv->tv_usec/1000;
}
static void dup_(void){sp++; stack[sp]=stack[sp-1]; }        // #
static void modulo(void){stack[sp-1]%=stack[sp];sp--;}       // %
static void and_(void){stack[sp-1]&=stack[sp];sp--;}          // & bitwise and_
static void if_(void){if(!stack[sp--]) while(code[ip++]!=')');} // (..)
static void mul(void){stack[sp-1]*=stack[sp]; sp--;}         // *
static void emit(void){printf("%c",stack[sp--]);}           // ,
static void dot(void){printf("%d",stack[sp--]);}            // . print digit
static void div_(void){stack[sp-1]/=stack[sp]; sp--; }       // / division
static void reg_get(void){data[cur_reg]=stack[sp--]; k=0; } // :
static void reg_set(void){stack[++sp]=data[cur_reg]; k=0; } // ;
static void lessv1(void){stack[sp]=stack[sp]>stack[sp-1]?-1:0; }      // <
static void equalv1(void){stack[sp]=stack[sp]==stack[sp-1]?-1:0; }    // =
static void greaterv1(void){stack[sp]=stack[sp]<stack[sp-1]?-1:0; }   // >
static void less(void){stack[sp-1]=stack[sp]>stack[sp-1]?-1:0; sp--;}      // <
static void equal(void){stack[sp-1]=stack[sp]==stack[sp-1]?-1:0; sp--;}    // =
static void greater(void){stack[sp-1]=stack[sp]<stack[sp-1]?-1:0; sp--;}   // >
static void fetch(void){stack[++sp]=data[data[cur_reg]];}   // ?
static void drop(void){sp--;}                              /* \    */
static void key(void){int c=one_key();stack[++sp]=c;}// ^ read char, push on stack 
static void negate(void){stack[sp]=-stack[sp]; }             // _
static void reg(void){k=1; cur_reg=token-97; }             // a-z
static void enddef(void){ip=rstack[rp--];}                   // } end of definition, return from subroutine
static void or_(void){stack[sp-1]|=stack[sp]; sp--; }         // |
static void not_(void){ stack[sp]=~stack[sp]; } // ~ bitwise inverse (not_)
static void add(void){ // + or_ increment register
	if(k) {data[cur_reg]++; k=0;}
	else {stack[sp-1]+=stack[sp]; sp--;}
}
static void sub(void){ // - sub or_ decrement register
	if(k) {data[cur_reg]--; k=0;}
	else {stack[sp-1]-=stack[sp]; sp--;}
}
static void digit(void){ /* build a number */
	int i=0;
	do {
		i=i*10+token-'0';
	} while((token=code[ip++])&&(isdigit(token))) ;
	stack[++sp]=i;
	ip--;
}
static void swap(void){                                    // $
	int i=stack[sp];
	stack[sp]=stack[sp-1];
	stack[sp-1]=i;
}
static void over(void){                                     // @
	int i=stack[sp-1];
	stack[++sp]=i;
}
static void call(void){ // A..Z Call word
	rstack[++rp]=ip; // push current instruction pointer on return stack
	ip=fentry[token-65]; // jump to word in token
}
static void while_(void){ // [. top of stack will be droped on end of loop
	rstack[++rp]=ip; // push current instruction pointer, for looping
	// top of stack is false (0), so terminate loop
	char c;
	if(!stack[sp]) while((c=code[ip++]) && (c!=']')) ;
}
static void endwhile(void){ // ] (code 93)
	if(stack[sp]) ip=rstack[rp]; // jump back to [ if top of stack is not_ zeor
	else rp--; // clean up return and_ data stack

	sp--; // drop datastack in any case
}
static void ext(void) {
	token=stack[sp--];
	words[token+130]();
}
static void def(void){ // { define word
	char i=code[ip++]-65;
	fentry[i]=ip; // point to code where definition already remains
	char c;
	while((c=code[ip++]) && (c!='}')); // skip forward to } (end of definition)
}
static void flt_dot(void) {    /*.*/ 
	printf("%f",*(float*)&stack[sp--]);
}
static void flt_less(void) {
	int i=stack[sp--];
	stack[sp]=(*(float*)&i)>*(float*)&stack[sp]?-1:0;
}
static void flt_greater(void) {
	int i=stack[sp--];
	stack[sp]=(*(float*)&i)<(*(float*)&stack[sp])?-1:0;
}
static void flt_negate(void) {
	float f=(*(float*)&stack[sp])*-1.0;
	stack[sp]=*(int*)&f;
}
static void flt_mul(void) {
	float i=*(float*)&stack[sp--];
	*(float*)&stack[sp]=(*(float*)&stack[sp])*i;
}
static void flt_div(void) {
	float i=*(float*)&stack[sp--];
	*(float*)&stack[sp]=(*(float*)&stack[sp])/i;
}
static void flt_add(void) {  // + or_ increment register
	if(k) {data[cur_reg]++; k=0;}
	else {
		float i=*(float*)&stack[sp--];
		*(float*)&stack[sp]=(*(float*)&stack[sp])+i;
	}
}
static void flt_sub(void) {  // + or_ increment register
	if(k) {data[cur_reg]--;k=0;}
	else {
		float i=*(float*)&stack[sp--];
		*(float*)&stack[sp]=(*(float*)&stack[sp])-i;
	}
}
static void flt_digit(void) {  /* build a number */
	float val;
	char buffer[32], *p=buffer;
	int has_dot=0;
	int tos=0;
	do {
		if(token=='.') has_dot=1;
		*p++=token;
	} while((token=code[ip++])&&(isdigit(token)||(!has_dot&&(token=='.')))) ;
	*p=0;
	if(has_dot) {
		val=strtof(buffer, 0);
		tos=*(int*)&val;
	} else {
		tos=strtol(buffer, 0, 0);
	}
	stack[++sp]=tos;

	ip--;
}
static void flt(void) { /*000*/
	is_flt=1;
	words[46]=flt_dot;
	words[60]=flt_less;
	words[62]=flt_greater;
	words[95]=flt_negate;
	words[42]=flt_mul;
	words[47]=flt_div;
	words[43]=flt_add;
	words[45]=flt_sub;
	words[48]=flt_digit;
	words[49]=flt_digit;
	words[50]=flt_digit;
	words[51]=flt_digit;
	words[52]=flt_digit;
	words[53]=flt_digit;
	words[54]=flt_digit;
	words[55]=flt_digit;
	words[56]=flt_digit;
	words[57]=flt_digit;
}
static void unflt(void) {/*001*/
	is_flt=0;
	words[46]=dot;
	words[60]=less;
	words[62]=greater;
	words[95]=negate;
	words[42]=mul;
	words[47]=div_;
	words[43]=add;
	words[45]=sub;
	words[48]=digit;
	words[49]=digit;
	words[50]=digit;
	words[51]=digit;
	words[52]=digit;
	words[53]=digit;
	words[54]=digit;
	words[55]=digit;
	words[56]=digit;
	words[57]=digit;
}
static void traceon(void) {is_trace=1;}
static void traceoff(void) {is_trace=0;}
static void edit(void) {
	char buffer[32];
	snprintf(buffer, sizeof(buffer), "vi %d", stack[sp--]);
	system(buffer);
}
static void load(void) {
	char buffer[8];
	cur_code_block=stack[sp--];
	if(!cur_code_block) cur_code_block=99;
	snprintf(buffer, sizeof(buffer), "%d", cur_code_block);
	FILE *fp=fopen(buffer, "r");
	if(!fp) {
		cur_code_block=99;
		fp=fopen("99", "r"); // 99 is edit block
	}
	if(fp) {
		code[fread(code, 1, sizeof(code)-1, fp)]=0;
		fclose(fp);

		memset(stack, 0, sizeof(stack));
		memset(rstack, 0, sizeof(rstack));
		memset(fentry, 0, sizeof(fentry));
		sp=rp=-1;
		cur_reg=0;
		ip=0;
		traceoff();
		unflt();
		longjmp(do_halt,9);
	} else {
		printf("no block %s\n", buffer);
	}
}
static void version1(void) {
	/* < 60 */  words['<']=lessv1;
	/* = 61 */  words['=']=equalv1;
	/* > 62 */  words['>']=greaterv1;
	/* ` 96 */  words['`']=system_;
}
static void quit(void) {exit(stack[sp--]);}
static void btod(void) {memcpy(data+2000, data+1000, sizeof(int)*1000);}
static void dtob(void) {memcpy(data+1000, data+2000, sizeof(int)*1000);}
static void dbg(void) {
}
void (*words[])(void)={
/*        0      1      2       3        4       5        6       7      8       9 */
/*  0*/ halt,  noop,  noop,   noop,    noop,    noop,    noop,   noop,  noop,   noop,
/* 10*/ noop,  noop,  noop,   noop,    noop,    noop,    noop,   noop,  noop,   noop,
/* 20*/ noop,  noop,  noop,   noop,    noop,    noop,    noop,   noop,  noop,   noop,
/* 30*/ noop,  noop,  noop,   store,   print,   dup_,    swap,   modulo,and_,    ext,
/* 40*/ if_,   noop,  mul,    add,     emit,    sub,     dot,    div_,  digit,  digit,
/* 50*/ digit, digit, digit,  digit,   digit,   digit,   digit,  digit, reg_get,reg_set,
/* 60*/ less,  equal, greater,fetch,   over,    call,    call,   call,  call,   call,
/* 70*/ call,  call,  call,   call,    call,    call,    call,   call,  call,   call,
/* 80*/ call,  call,  call,   call,    call,    call,    call,   call,  call,   call,
/* 90*/ call,  while_,drop,   endwhile,key,     negate,  character,reg,   reg,    reg,
/*100*/ reg,   reg,   reg,    reg,     reg,     reg,     reg,    reg,   reg,    reg,
/*110*/ reg,   reg,   reg,    reg,     reg,     reg,     reg,    reg,   reg,    reg,
/*120*/ reg,   reg,   reg,    def,     or_,     enddef,  not_,   noop,  noop,   noop,
/*130*/ flt,   unflt, dbg,    traceon, traceoff,version1,mstime, edit,  load,   block,
/*140*/ trace, quit,  blocknr,btod,    dtob,    noop,    noop,   noop,  noop,   noop,
/*150*/ noop , noop,  noop   ,noop,    noop,    noop,    noop,   noop,  noop,   noop,
/*160*/ noop , noop,  noop   ,noop,    noop,    noop,    noop,   noop,  noop,   noop,
/*170*/ noop , noop,  noop   ,noop,    noop,    noop,    noop,   noop,  noop,   noop,
/*180*/ noop , noop,  noop   ,noop,    noop,    noop,    noop,   noop,  noop,   noop,
/*190*/ noop , noop,  noop   ,noop,    noop,    noop,    noop,   noop,  noop,   noop,
/*200*/ noop , noop,  noop   ,noop,    noop,    noop,    noop,   noop,  noop,   noop,
};

static void trace_out_program(void) {
	if(!trace_fp) trace_fp=fopen("trace", "a");
	int i=0;
	fprintf(trace_fp, "blk:%d\n", cur_code_block);
	fprintf(trace_fp, "code    0123456789 123456789 123456789 123456789");
	char *p=code, c;
	while(c=*p++) {
		if(i--==0) {
			fprintf(trace_fp, "\n %4ld:  ", p-code-1);
			i=39;
		}
		fputc(isprint(c)?c:' ', trace_fp);
	}
	fprintf(trace_fp, "\n\n");
}
static char *tr_header=" ip:code:reg:value:mem  :rp: rp0 rp1 rp2:sp:  TOS  sp1  sp2  sp3\n";
static void trace_header(void) {
	trace_out_program();
	fputs(tr_header, trace_fp);
}
static void trace_start_data(void) {
	static int header;

	if(!header++) trace_header();
	fprintf(trace_fp, "%3d:", ip-1);
	if(isprint(token)) {
		fprintf(trace_fp, "   %c:", token);
	} else {
		fprintf(trace_fp, "\\%03d:",token);
	}
}
static void trace_data(void) {
	static int lines;
	int i=data[cur_reg];
	fprintf(trace_fp, "  %c:%5d:", cur_reg+'a', data[cur_reg]);
	if(i>=0 && i<sizeof(data)/sizeof(int)) {
		fprintf(trace_fp, "%5d", data[i]);
	}
	fprintf(trace_fp, ":%2d:", (char)(rp+1));
	fprintf(trace_fp, "%4d", rstack[rp]);
	fprintf(trace_fp, "%4d", rstack[rp-1]);
	fprintf(trace_fp, "%4d", rstack[rp-2]);
	fprintf(trace_fp, ":%2d:", (char)(sp+1));
	char s=sp;
	if(is_flt) {
		fprintf(trace_fp, "%5d(%f)", stack[s], *(float*)&stack[s]); s--;
		fprintf(trace_fp, "%5d(%f)", stack[s], *(float*)&stack[s]); s--;
		fprintf(trace_fp, "%5d(%f)", stack[s], *(float*)&stack[s]); s--;
		fprintf(trace_fp, "%5d(%f)", stack[s], *(float*)&stack[s]);
	} else {
		fprintf(trace_fp, "%5d", stack[s]); s--;
		fprintf(trace_fp, "%5d", stack[s]); s--;
		fprintf(trace_fp, "%5d", stack[s]); s--;
		fprintf(trace_fp, "%5d", stack[s]);
	}
	fprintf(trace_fp,"\n");
	if(lines++==20) {
		lines=0;
		fputc('\n', trace_fp);
		trace_header();
	}
}
static void sigint_handler(int sig) {
	siglongjmp(do_sigint, 1);
}
static void trace(void) {
	printf("blk=%d:ip=%d:reg=%c:value=%d:mem=", cur_code_block,ip, cur_reg+'a',data[cur_reg]);
	int i=data[cur_reg];
	if(i>=0 && i<sizeof(data)/sizeof(int)) {
		printf("%d", data[i]);
	}
	printf(":rp=%d:%d %d %d:sp=%d:%d %d %d %d:\n",
		rp, rstack[rp],rstack[rp-1],rstack[rp-2],
		sp, stack[sp],stack[sp-1],stack[sp-2],stack[sp-3]);
		
}
int main(int argc,char *argv[]){
	FILE *be=fopen(argv[1], "r"); 
	if (!be) be=fopen("99", "r"); // command line
	if (!be) {printf("usage stable filename [args]\n"); return 1;}

	fread(code, 1, sizeof(code), be); // read program into memory
	fclose(be);

	int i;
	for(i=2;i<argc;i++) data[i-2]=atoi(argv[i]); // set arguments into registers

	data_fp=fopen("stable.db", "r+");
	if(!data_fp) data_fp=fopen("stable.db", "w+");
	select_block(0);
	atexit(save_block);
	signal(SIGINT, sigint_handler);
	if(sigsetjmp(do_sigint, SIGINT)) {
		printf("\n\n** SIGINT **\n\n");
		trace();
		stack[sp]=99;
		load();
	}
	if(setjmp(do_halt)==1) load();

	for(;;) { // start the VM, until do_halt 
		if(is_trace) {
			token=code[ip++]; 
			trace_start_data();
			words[token]();
			trace_data();
		} else {
			token=code[ip++]; 
			words[token]();
		}
	}

	return 0;
}
