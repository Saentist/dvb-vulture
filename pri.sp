@initialize:python@
@@
coccinelle.pd={}

@r@
typedef int8_t,int16_t,int32_t,int64_t,uint8_t,uint16_t,uint32_t,uint64_t;
{int8_t,int16_t,int32_t,int64_t,
uint8_t,uint16_t,uint32_t,uint64_t} x;
type T;
T y;
expression list[n] es;
constant char[] c;
identifier f =~ ".*printf$";
position p;
@@

f(...,c@p,es,x@y,...)

@script:python collect@
p << r.p;  // position
f << r.f;  // func name
T << r.T;  // the type of the argument of interest
c << r.c;  // the format string
n << r.n;  // the offset of the argument of interest, counted from 0
@@
import re
trex = re.compile("((uint\\S*_t)|(int\\S*_t))")//to match only typename(no qualifiers)
m=trex.search(T)
t=m.group(1)
ps=("%s:(%s,%s),(%s,%s)::" % (p[0].file,p[0].line,p[0].column,p[0].line_end,p[0].column_end))
if (ps in coccinelle.pd):
	coccinelle.pd[ps][1][n]=t
else:
	coccinelle.pd[ps]=[c,{n:t}]

@script:python fmt@
p << r.p;  // position
c2;
@@
ps=("%s:(%s,%s),(%s,%s)::" % (p[0].file,p[0].line,p[0].column,p[0].line_end,p[0].column_end))
in_str=False//inside string lit
in_pesc=False//seen a %
in_besc=False//seen a \
tps=coccinelle.pd[ps][1]//the types
import re
prex = re.compile("\s*(PRI([diouxX])(8|16|32|64))\s*")//to match existing ones...
pmap= {
"int8_t"  :{"c":"\"c\"","d":"PRId8",  "i":"PRIi8",  "o":"PRIo8",  "u":"PRIu8",  "x":"PRIx8",  "X":"PRIX8"},
"int16_t" :{"d":"PRId16", "i":"PRIi16", "o":"PRIo16", "u":"PRIu16", "x":"PRIx16", "X":"PRIX16"},
"int32_t" :{"d":"PRId32", "i":"PRIi32", "o":"PRIo32", "u":"PRIu32", "x":"PRIx32", "X":"PRIX32"},
"int64_t" :{"d":"PRId64", "i":"PRIi64", "o":"PRIo64", "u":"PRIu64", "x":"PRIx64", "X":"PRIX64"},
"uint8_t" :{"c":"\"c\"","d":"PRIu8",  "i":"PRIu8",  "o":"PRIo8",  "u":"PRIu8",  "x":"PRIx8",  "X":"PRIX8"},
"uint16_t":{"d":"PRIu16", "i":"PRIu16", "o":"PRIo16", "u":"PRIu16", "x":"PRIx16", "X":"PRIX16"},
"uint32_t":{"d":"PRIu32", "i":"PRIu32", "o":"PRIo32", "u":"PRIu32", "x":"PRIx32", "X":"PRIX32"},
"uint64_t":{"d":"PRIu64", "i":"PRIu64", "o":"PRIo64", "u":"PRIu64", "x":"PRIx64", "X":"PRIX64"},
}
fmt_ctr=0//some index
str_a=coccinelle.pd[ps][0]
coccinelle.c2=""
i=0
a=-1
while True:
	if i>=len(str_a):
		break
	c=str_a[i]
	if in_str==True:
		if in_pesc==True:
			if c=="%":
				in_pesc=False
				coccinelle.c2="%s%s" % (coccinelle.c2,c)
				i+=1
			else:
				if c in "hlLqjzt"://size modifiers. mem index of first one
					if a==-1:
						a=i
					i+=1
				elif c in "diouxXEeFfGgAacsCSP":
					in_pesc=False
					if a==-1:
						a=i
					fs="%s" % (fmt_ctr)
					if fs in tps:
						coccinelle.c2="%s\"%s\"" % (coccinelle.c2,pmap[tps[fs]][c])
					else:
						coccinelle.c2="%s%s" % (coccinelle.c2,str_a[a:(i+1)])
					fmt_ctr+=1
					i+=1
				else://just append the prec stuff...
					if c=="\"":
						in_str=False
					coccinelle.c2="%s%s" % (coccinelle.c2,c)
					i+=1
		elif in_besc==True:
			in_besc=False
			coccinelle.c2="%s%s" % (coccinelle.c2,c)
			i+=1
		else:
			if c=="\\":
				in_besc=True
			elif c=="%":
				in_pesc=True
				a=-1
			elif c=="\"":
				in_str=False
			coccinelle.c2="%s%s" % (coccinelle.c2,c)
			i+=1
	else:
		if c=="\"":
			in_str=True
			coccinelle.c2="%s%s" % (coccinelle.c2,c)
			i+=1;
		else:
			if in_pesc==True:
				m=prex.match(str_a[i:])
				fs="%s" % (fmt_ctr)
				if m and (fs in tps):
					i+=m.end();
					coccinelle.c2="%s%s" % (coccinelle.c2,pmap[tps[fs]][m.group(2)])
					fmt_ctr+=1
					in_pesc=False
				else:
					coccinelle.c2="%s%s" % (coccinelle.c2,c)
					i+=1
			else:
				coccinelle.c2="%s%s" % (coccinelle.c2,c)
				i+=1;
@q@
expression e;
position r.p;
identifier fmt.c2;
@@
- e@p
+ c2




