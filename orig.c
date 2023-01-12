/* stable.c  interpreter: ver: 01.sep.2016 */
#include <stdio.h>
static int  i,v,m,r,p=0,n,mx,s,f,st[2000]; 
static char t=0,c,u,a,k=0,pr[2000];
N(){;} f33(){st[st[v]]=st[s]; s--; }
f34(){p++;u=pr[p];while(u!='\"'){putc(u,stdout);p++;u=pr[p];}}
f35(){s++; st[s]=st[s-1];}
f36(){i=st[s]; st[s]=st[s-1]; st[s-1]=i;}
f37(){st[s-1]%=st[s]; s--; } f38(){st[s-1]&=st[s]; s--; }
f39(){p++;u=pr[p];while(u!='\''){s++;st[s]=u;p++;u=pr[p];}}
f40(){if(st[s]==0){s--;p++;u=pr[p];while(u!=')'){p++;u=pr[p];}} else{s--;}}
f42(){st[s-1]*=st[s]; s--; }
f43(){ if(k==0){st[s-1]+=st[s]; s--; }else{st[v]++;} }
f44(){printf("%c",st[s]); s--; }
f45(){ if(k==0){st[s-1]-=st[s]; s--; }else{st[v]--;} }
f46(){printf("%d",st[s]); s--;} f47(){st[s-1]/=st[s]; s--; }
f9(){i=0;while((u>='0')&&(u<='9')){i=i*10+u-'0';p++;u=pr[p];}s++;st[s]=i;p--;}
f58(){st[v]=st[s]; s--; } f59(){s++; st[s]=st[v]; }
f60(){if(st[s]> st[s-1]){st[s]=-1;}else{st[s]=0;}}
f61(){if(st[s]==st[s-1]){st[s]=-1;}else {st[s]=0;}}
f62(){if(st[s]< st[s-1]){st[s]=-1;}else{st[s]=0;}}
f63(){s++; st[s]=st[st[v]]; }
f64(){i=st[s];st[s]=st[s-2];st[s-2]=st[s-1];st[s-1]=i;}
fA(){r++; st[r]=p; p=st[u-35]; u=pr[p]; p--;}
f91(){r++;st[r]=p;if(st[s]==0){p++;u=pr[p]; while(u!=']'){ p++; u=pr[p];}}}
f92(){s--;} f93(){if(st[s]!=0) p=st[r]; else r--; s--; }
f94(){if((c=getc(stdin))==EOF) { c=0; } s++; st[s]=c; }
f95(){st[s]=-st[s]; }f96(){putc(u,stdout);} fa(){k=1; v=u-97; }
f123(){f=pr[p+1]-35;st[f]=p+2;while(u!='}'){p++;u=pr[p];}}
f124(){st[s-1]|=st[s]; s--; }f125(){p=st[r];r--;}f126(){st[s]=  ~st[s]; }
int (*q[127])()={
N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,
f33,f34,f35,f36,f37,f38,f39,f40,N,f42,f43,f44,f45,f46,f47,f9,f9,f9,
f9,f9,f9,f9,f9,f9,f9,f58,f59,f60,f61,f62,f63,f64,fA,fA,fA,fA,fA,fA,
fA,fA,fA,fA,fA,fA,fA,fA,fA,fA,fA,fA,fA,fA,fA,fA,fA,fA,fA,fA,f91,f92,
f93,f94,f95,f96,fa,fa,fa,fa,fa,fa,fa,fa,fa,fa,fa,fa,fa,fa,fa,fa,fa,
fa,fa,fa,fa,fa,fa,fa,fa,fa,f123,f124,f125,f126};
int main(int argc,char *argv[]){ FILE *be; 
 if (!(be=fopen(argv[1],"rb"))) {printf("pr?\n"); return 0;}
 while((c=fgetc(be))!=EOF){ pr[p++]=c; } 
 fclose(be); mx=p; i=1999; while(i--){ st[i]=0;}  
 for(i=0;i<argc;i++) if(i>1) st[i-2]=atoi(argv[i]);
 p=0; s=100; r=60; 
 while(p<=mx){ u=pr[p]; q[u](); if(u<'a')k=0; else if(u>'z')k=0;
 p++;} return 0;}
