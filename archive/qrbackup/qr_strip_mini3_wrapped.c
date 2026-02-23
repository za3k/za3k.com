#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<stdint.h>
#include<math.h>
typedef uint8_t U;typedef uint32_t V;typedef 
uint16_t W;typedef struct{float x,y;}du;typedef 
struct{float h[3][3];}bF;typedef struct{int G,aH;
float Z;bF fs;}aJ;typedef struct{int aT;int bf;U*l;}
bG;typedef struct{char bp;int aE;int aA;U*l;int D;}
cY;typedef struct{cY*I;int F;int aF;}av;static int 
fG(FILE*f){int c;while((c=fgetc(f))!=EOF){if(c=='#')
while((c=fgetc(f))!=EOF&&c!='\n');else if(c>' '){
ungetc(c,f);return 1;}}return 0;}static void dW(U*dl
,U*hP,int aT,int bf){int bs=32;for(int by=0;by<bf;by
+=bs){for(int bx=0;bx<aT;bx+=bs){int bw=(bx+bs*2<aT)
?bs*2:aT-bx;int bh=(by+bs*2<bf)?bs*2:bf-by;int hx=0;
for(int y=by;y<by+bh;y++)for(int x=bx;x<bx+bw;x++)hx
+=dl[y*aT+x];int gs=hx/(bw*bh)-10;if(gs<20)gs=20;int
ew=(bx+bs<aT)?bs:aT-bx;int eh=(by+bs<bf)?bs:bf-by;
for(int y=by;y<by+eh;y++)for(int x=bx;x<bx+ew;x++){
int gE=y*aT+x;hP[gE]=(dl[gE]<gs)?1:0;}}}}static bG*
fz(const char*hl){FILE*f=fopen(hl,"rb");if(!f)return
0;bG*ag=calloc(1,sizeof(bG));U*dl=0;char eZ[3]={0};
if(fread(eZ,1,2,f)!=2)goto eC;int gd=0;if(eZ[1]=='1'
)gd=1;else if(eZ[1]=='4')gd=4;else if(eZ[1]=='2')gd=
2;else if(eZ[1]=='5')gd=5;if(eZ[0]!='P'||!gd)goto eC
;if(!fG(f)||fscanf(f,"%d",&ag->aT)!=1)goto eC;if(!fG
(f)||fscanf(f,"%d",&ag->bf)!=1)goto eC;int eD=1;if(
gd==2||gd==5){if(!fG(f)||fscanf(f,"%d",&eD)!=1)goto 
eC;if(eD<=0)eD=255;}fgetc(f);int n=ag->aT*ag->bf;ag
->l=malloc(n);if(gd==4){int rb=(ag->aT+7)/8;U*fa=
malloc(rb);for(int y=0;y<ag->bf;y++){if(fread(fa,1,
rb,f)!=(size_t)rb){free(fa);goto eC;}for(int x=0;x<
ag->aT;x++)ag->l[y*ag->aT+x]=(fa[x/8]>>(7-x%8))&1;}
free(fa);}else if(gd==1){for(int i=0;i<n;i++){int c;
while((c=fgetc(f))!=EOF&&c<=' ');if(c==EOF)goto eC;
ag->l[i]=(c=='1')?1:0;}}else{dl=malloc(n);if(gd==5){
if(fread(dl,1,n,f)!=(size_t)n)goto eC;}else{for(int 
i=0;i<n;i++){int fH;if(fscanf(f,"%d",&fH)!=1)goto eC
;dl[i]=fH;}}if(eD!=255)for(int i=0;i<n;i++)dl[i]=dl[
i]*255/eD;dW(dl,ag->l,ag->aT,ag->bf);free(dl);dl=0;}
fclose(f);return ag;eC:free(dl);if(ag)free(ag->l);
free(ag);fclose(f);return 0;}static void gF(bG*ag){
if(ag){free(ag->l);free(ag);}}static int cZ(bG*ag,
int x,int y){if(x<0||x>=ag->aT||y<0||y>=ag->bf)
return 0;return ag->l[y*ag->aT+x];}typedef struct{
int x,y;float Z;float fI;float fJ;}ac;typedef struct
{int E;int aK;int aL;}X;typedef struct{int eE,eF;int
cD,cE;int cF;int F;float bY;}aa;static float da(int*
J,float gR){int aA=J[0]+J[1]+J[2]+J[3]+J[4];if(aA<7)
return 0;if(!J[0]||!J[1]||!J[2]||!J[3]||!J[4])return
0;float dN=aA/7.0f,im=dN*gR*1.2f;float eb=(float)J[2
]/aA;if(eb<0.25f||eb>0.55f)return 0;for(int i=0;i<5;
i+=(i==1?2:1))if(J[i]<dN-im||J[i]>dN+im)return 0;if(
J[2]<2.0f*dN||J[2]>4.0f*dN)return 0;return dN;}
static int fb(X*ej,int F,int io,int E,int ip,int iq)
{if(F<io){ej[F].E=E;ej[F].aK=ip;ej[F].aL=iq;}return 
F<io?F+1:F;}static int fA(int a0,int a1,int b0,int 
b1){int ov=((a1<b1?a1:b1)-(a0>b0?a0:b0));int hy=((a1
-a0)<(b1-b0))?(a1-a0):(b1-b0);return ov>=0&&ov>=hy/2
;}static int bO(X*ej,int dO,aa**ft,int*ec){if(dO==0)
return 0;aa*bg=*ft;int aF=*ec;int fK=0;typedef 
struct{int ek;int dm;int dn;int co;}fL;fL br[128];
int dv=0;int co=-999;for(int i=0;i<dO;i++){X*line=&
ej[i];if(line->E!=co){int dc=0;for(int a=0;a<dv;a++)
{if(line->E-br[a].co<=3+1){if(dc!=a){br[dc]=br[a];}
dc++;}}dv=dc;co=line->E;}int eG=-1;int eH=(line->aK+
line->aL)/2;for(int a=0;a<dv;a++){if(fA(line->aK,
line->aL,br[a].dm,br[a].dn)){int iQ=line->E-br[a].co
;if(iQ>1){aa*r=&bg[br[a].ek];int dX=r->cD;int dY=r->
cE;if(eH<dX-10||eH>dY+10){continue;}}eG=a;break;}}
int ri,ai;if(eG>=0){ri=br[eG].ek;ai=eG;}else if(dv<
128){if(fK>=aF){aF*=2;bg=realloc(bg,aF*sizeof(aa));*
ft=bg;*ec=aF;}ri=fK++;ai=dv++;br[ai].ek=ri;bg[ri].eE
=line->E;bg[ri].cD=line->aL;bg[ri].cE=line->aK;bg[ri
].cF=0;bg[ri].bY=0;bg[ri].F=0;}else continue;aa*r=&
bg[ri];r->eF=line->E;if(line->aK<r->cD)r->cD=line->
aK;if(line->aL>r->cE)r->cE=line->aL;r->cF+=(line->aK
+line->aL)/2;r->bY+=(line->aL-line->aK)/7.0f;r->F++;
br[ai].dm=line->aK;br[ai].dn=line->aL;br[ai].co=line
->E;}int hz=0;for(int i=0;i<fK;i++){aa*r=&bg[i];int 
el=(int)(r->bY/r->F*1.4f);if(el<3)el=3;if(r->F>=el&&
r->eF-r->eE>=4)bg[hz++]=*r;}return hz;}static int aS
(const void*a,const void*b){const X*la=(const X*)a;
const X*lb=(const X*)b;return la->E-lb->E;}static 
int cq(bG*ag,X*ej,int hA){int ic=hA?ag->aT:ag->bf;
int id=hA?ag->bf:ag->aT,dO=0;for(int o=0;o<ic;o++){
int J[5]={0},q=0;for(int i=0;i<id;i++){int hB=hA?cZ(
ag,o,i):cZ(ag,i,o);int hn=(q&1);if(q==0){if(hB){q=1;
J[0]=1;}}else if(hB==hn){J[q-1]++;}else if(q<5){q++;
J[q-1]=1;}else{if(da(J,0.8f)>0){int w=J[0]+J[1]+J[2]
+J[3]+J[4];dO=fb(ej,dO,16000,o,i-w,i-1);}J[0]=J[2];J
[1]=J[3];J[2]=J[4];J[3]=1;J[4]=0;q=4;}}}return dO;}
static int dq(bG*ag,ac*ah,int ed){X*eI=malloc(16000*
sizeof(X));X*eJ=malloc(16000*sizeof(X));int ir=128,
is=128;aa*fc=malloc(ir*sizeof(aa));aa*fd=malloc(is*
sizeof(aa));int it=cq(ag,eI,0);int iu=cq(ag,eJ,1);
qsort(eI,it,sizeof(X),aS);qsort(eJ,iu,sizeof(X),aS);
int iR=bO(eI,it,&fc,&ir);int iS=bO(eJ,iu,&fd,&is);
int F=0;for(int hi=0;hi<iR&&F<ed;hi++){aa*hr=&fc[hi]
;int fx=hr->cF/hr->F;float hm=hr->bY/hr->F;for(int 
vi=0;vi<iS&&F<ed;vi++){aa*vr=&fd[vi];int fy=vr->cF/
vr->F;float vm=vr->bY/vr->F;if(fx<vr->eE-hm*1.5f||fx
>vr->eF+hm*1.5f)continue;if(fy<hr->eE-vm*1.5f||fy>hr
->eF+vm*1.5f)continue;float ie=(hm>vm)?hm/vm:vm/hm;
if(ie>((hm>3.0f||vm>3.0f)?3.0f:2.5f))continue;float 
fm=sqrtf(hm*vm);int gt=0;for(int i=0;i<F&&!gt;i++){
int dx=ah[i].x-fx,dy=ah[i].y-fy;gt=dx*dx+dy*dy<fm*fm
*16;}if(!gt){ah[F].x=fx;ah[F].y=fy;ah[F].Z=fm;ah[F].
fI=hm;ah[F].fJ=vm;F++;}}}free(eI);free(eJ);free(fc);
free(fd);return F;}static int dZ(int v){return 17+4*
v;}typedef struct{U eK[177][177];int aH;int G;}fE;
static void cN(int G,int E[8]){if(G==1){E[0]=0;
return;}int aH=17+G*4,hX=aH-7,hQ=G/7+2;int aA=hX-6,
iv=hQ-1;int iw=((aA*2+iv)/(iv*2)+1)&~1;E[0]=6;for(
int i=hQ-1;i>=1;i--)E[i]=hX-(hQ-1-i)*iw;E[hQ]=0;}
static int ck(int G,int x,int y){int s=dZ(G);if((x<9
&&y<9)||(x<8&&y>=s-8)||(x>=s-8&&y<9))return 1;if((y
==8&&(x<9||x>=s-8))||(x==8&&(y<9||y>=s-8)))return 1;
if(x==6||y==6)return 1;if(G>=2){int E[8];cN(G,E);for
(int i=0;E[i];i++)for(int j=0;E[j];j++){int ax=E[i],
ay=E[j];if((ax<9&&ay<9)||(ax<9&&ay>=s-8)||(ax>=s-8&&
ay<9))continue;if(x>=ax-2&&x<=ax+2&&y>=ay-2&&y<=ay+2
)return 1;}}if(G>=7){if((x<6&&y>=s-11&&y<s-8)||(y<6
&&x>=s-11&&x<s-8))return 1;}return 0;}static int ar(
bG*ag,const aJ*O,int mx,int my,int hf);static int cG
(fE*qr,bG*ag,const aJ*O){static const int xs[15]={8,
8,8,8,8,8,8,8,7,5,4,3,2,1,0};static const int ys[15]
={0,1,2,3,4,5,7,8,8,8,8,8,8,8,8};V fM=0;for(int i=14
;i>=0;i--){int gG;if(ag)gG=ar(ag,O,xs[i],ys[i],0);
else gG=qr->eK[ys[i]][xs[i]]&1;fM=(fM<<1)|gG;}return
fM^0x5412;}static const W bH[32]={0x0000,0x0537,
0x0a6e,0x0f59,0x11eb,0x14dc,0x1b85,0x1eb2,0x23d6,
0x26e1,0x29b8,0x2c8f,0x323d,0x370a,0x3853,0x3d64,
0x429b,0x47ac,0x48f5,0x4dc2,0x5370,0x5647,0x591e,
0x5c29,0x614d,0x647a,0x6b23,0x6e14,0x70a6,0x7591,
0x7ac8,0x7fff};static int ap(int am){int em=am;for(
int i=14;i>=10;i--){if(em&(1<<i))em^=(0x537<<(i-10))
;}if(em==0)return am;int fu=-1,dz=5;for(int i=0;i<32
;i++){int hC=am^bH[i];int fe=0;while(hC){fe+=hC&1;hC
>>=1;}if(fe<dz){dz=fe;fu=bH[i];}}return fu;}static U
bI[512],dP[256];static void cR(void){static int gH=0
;if(gH)return;gH=1;int x=1;for(int i=0;i<255;i++){bI
[i]=x;dP[x]=i;x=(x<<1)^((x>>7)*0x11d);}for(int i=255
;i<512;i++)bI[i]=bI[i-255];}static U bt(U a,U b){
return(a&&b)?bI[dP[a]+dP[b]]:0;}static U gu(U a,U b)
{return(a&&b)?bI[(dP[a]+255-dP[b])%255]:0;}static U 
hR(U a){return a?bI[255-dP[a]]:0;}static void cr(U*l
,int bv,int aB,U*aj){cR();for(int i=0;i<aB;i++){U s=
0;for(int j=0;j<bv;j++){s=bt(s,bI[i])^l[j];}aj[i]=s;
}}static int dA(U*aj,int aB,U*cs){cR();U C[256]={0};
U B[256]={0};C[0]=1;B[0]=1;int L=0,m=1;U b=1;for(int
n=0;n<aB;n++){U d=aj[n];for(int i=1;i<=L;i++){d^=bt(
C[i],aj[n-i]);}if(d==0){m++;}else if(2*L<=n){U T[256
];memcpy(T,C,sizeof(T));U hD=gu(d,b);for(int i=0;i<
aB-m;i++){C[i+m]^=bt(hD,B[i]);}memcpy(B,T,sizeof(B))
;L=n+1-L;b=d;m=1;}else{U hD=gu(d,b);for(int i=0;i<aB
-m;i++){C[i+m]^=bt(hD,B[i]);}m++;}}memcpy(cs,C,aB+1)
;return L;}static int ff(U*cs,int Q,int n,int*aM){cR
();int fN=0;for(int i=0;i<n;i++){U hx=cs[0];for(int 
j=1;j<=Q;j++){U ig=((255-i)*j)%255;hx^=bt(cs[j],bI[
ig]);}if(hx==0)aM[fN++]=n-1-i;}return(fN==Q)?fN:-1;}
static void gS(U*aj,U*cs,int Q,int*aM,int n,U*eL){cR
();U hE[256]={0};for(int i=0;i<Q;i++){U v=0;for(int 
j=0;j<=i;j++){v^=bt(aj[i-j],cs[j]);}hE[i]=v;}U eM[
256]={0};for(int i=1;i<=Q;i+=2){eM[i-1]=cs[i];}for(
int i=0;i<Q;i++){int E=aM[i];int ee=(n-1-E)%255;U fO
=0;for(int j=0;j<Q;j++){fO^=bt(hE[j],bI[(ee*j)%255])
;}U bP=0;for(int j=0;j<Q;j++){int p=(ee*j)%255;bP^=
bt(eM[j],bI[p]);}if(bP!=0){eL[i]=gu(fO,bP);}else{eL[
i]=0;}}}static int en(U*l,int D,int aB){cR();int bv=
D+aB,gI=aB/2;U aj[256];cr(l,bv,aB,aj);int gj=1;for(
int i=0;i<aB;i++)if(aj[i]!=0){gj=0;break;}if(gj)
return 0;U cs[256]={0};int Q=dA(aj,aB,cs);if(Q>gI)
return-1;int aM[256];int fN=ff(cs,Q,bv,aM);if(fN!=Q)
return-1;U eL[256];gS(aj,cs,Q,aM,bv,eL);for(int i=0;
i<Q;i++){if(aM[i]>=0&&aM[i]<bv)l[aM[i]]^=eL[i];}cr(l
,bv,aB,aj);for(int i=0;i<aB;i++)if(aj[i]!=0)return-1
;return Q;}static int ho(int m,int x,int y){switch(m
){case 0:return(y+x)%2==0;case 1:return y%2==0;case 
2:return x%3==0;case 3:return(y+x)%3==0;case 4:
return(y/2+x/3)%2==0;case 5:return(y*x)%2+(y*x)%3==0
;case 6:return((y*x)%2+(y*x)%3)%2==0;default:return(
(y+x)%2+(y*x)%3)%2==0;}}static void gT(fE*qr,int iz)
{for(int y=0;y<qr->aH;y++)for(int x=0;x<qr->aH;x++)
if(!ck(qr->G,x,y)&&ho(iz,x,y))qr->eK[y][x]^=1;}
typedef struct{int bs,dw,ns;}dB;typedef struct{int 
hY;dB iT[4];}gv;static const gv ge[41]={{0},{26,{{26
,16,1},{26,19,1},{26,9,1},{26,13,1}}},{44,{{44,28,1}
,{44,34,1},{44,16,1},{44,22,1}}},{70,{{70,44,1},{70,
55,1},{35,13,2},{35,17,2}}},{100,{{50,32,2},{100,80,
1},{25,9,4},{50,24,2}}},{134,{{67,43,2},{134,108,1},
{33,11,2},{33,15,2}}},{172,{{43,27,4},{86,68,2},{43,
15,4},{43,19,4}}},{196,{{49,31,4},{98,78,2},{39,13,4
},{32,14,2}}},{242,{{60,38,2},{121,97,2},{40,14,4},{
40,18,4}}},{292,{{58,36,3},{146,116,2},{36,12,4},{36
,16,4}}},{346,{{69,43,4},{86,68,2},{43,15,6},{43,19,
6}}},{404,{{80,50,1},{101,81,4},{36,12,3},{50,22,4}}
},{466,{{58,36,6},{116,92,2},{42,14,7},{46,20,4}}},{
532,{{59,37,8},{133,107,4},{33,11,12},{44,20,8}}},{
581,{{64,40,4},{145,115,3},{36,12,11},{36,16,11}}},{
655,{{65,41,5},{109,87,5},{36,12,11},{54,24,5}}},{
733,{{73,45,7},{122,98,5},{45,15,3},{43,19,15}}},{
815,{{74,46,10},{135,107,1},{42,14,2},{50,22,1}}},{
901,{{69,43,9},{150,120,5},{42,14,2},{50,22,17}}},{
991,{{70,44,3},{141,113,3},{39,13,9},{47,21,17}}},{
1085,{{67,41,3},{135,107,3},{43,15,15},{54,24,15}}},
{1156,{{68,42,17},{144,116,4},{46,16,19},{50,22,17}}
},{1258,{{74,46,17},{139,111,2},{37,13,34},{54,24,7}
}},{1364,{{75,47,4},{151,121,4},{45,15,16},{54,24,11
}}},{1474,{{73,45,6},{147,117,6},{46,16,30},{54,24,
11}}},{1588,{{75,47,8},{132,106,8},{45,15,22},{54,24
,7}}},{1706,{{74,46,19},{142,114,10},{46,16,33},{50,
22,28}}},{1828,{{73,45,22},{152,122,8},{45,15,12},{
53,23,8}}},{1921,{{73,45,3},{147,117,3},{45,15,11},{
54,24,4}}},{2051,{{73,45,21},{146,116,7},{45,15,19},
{53,23,1}}},{2185,{{75,47,19},{145,115,5},{45,15,23}
,{54,24,15}}},{2323,{{74,46,2},{145,115,13},{45,15,
23},{54,24,42}}},{2465,{{74,46,10},{145,115,17},{45,
15,19},{54,24,10}}},{2611,{{74,46,14},{145,115,17},{
45,15,11},{54,24,29}}},{2761,{{74,46,14},{145,115,13
},{46,16,59},{54,24,44}}},{2876,{{75,47,12},{151,121
,12},{45,15,22},{54,24,39}}},{3034,{{75,47,6},{151,
121,6},{45,15,2},{54,24,46}}},{3196,{{74,46,29},{152
,122,17},{45,15,24},{54,24,49}}},{3362,{{74,46,13},{
152,122,4},{45,15,42},{54,24,48}}},{3532,{{75,47,40}
,{147,117,20},{45,15,10},{54,24,43}}},{3706,{{75,47,
18},{148,118,19},{45,15,20},{54,24,34}}}};static int
ea(U*hS,int bJ,int bc,int dw,int bs,int ns,int dd,U*
fP){int eN=(dd>=ns);int dQ=eN?dw+1:dw;int eo=bs-dw;
int hp=eN?bs+1:bs;int E=0;for(int j=0;j<dQ;j++){int 
ct;if(j<dw){ct=j*bc+dd;}else{ct=dw*bc+(dd-ns);}if(ct
<bJ){fP[E++]=hS[ct];}}int gJ=dw*bc+(bc-ns);for(int j
=0;j<eo;j++){int ct=gJ+j*bc+dd;if(ct<bJ){fP[E++]=hS[
ct];}}return hp;}static int cC(U*hS,int bJ,int G,int
ep,U*hq,int gK,int*cH,int*cu){if(G<1||G>40)return 0;
const dB*sb=&ge[G].iT[ep];int ns=sb->ns;int dw=sb->
dw;int bs=sb->bs;int gw=ns*bs;int M=bJ-gw;int nl=(M>
0)?M/(bs+1):0;int bc=ns+nl;int eo=bs-dw;int dR=0,ef=
0,aU=0;for(int i=0;i<bc&&dR<gK;i++){U aY[256];(void)
ea(hS,bJ,bc,dw,bs,ns,i,aY);int eN=(i>=ns);int dQ=eN?
dw+1:dw;int eq=en(aY,dQ,eo);if(eq<0){aU++;}else if(
eq>0){ef+=eq;}for(int j=0;j<dQ&&dR<gK;j++){hq[dR++]=
aY[j];}}if(cH)*cH=ef;if(cu)*cu=aU;if(aU==bc){return-
1;}return dR;}static int fg(fE*qr,U*l,int bZ){int aH
=qr->aH,bi=0,gG=7,up=1;memset(l,0,bZ);for(int cO=aH-
1;cO>=0;cO-=2){if(cO==6)cO--;int hT=up?aH-1:0;int iA
=up?-1:aH;int ih=up?-1:1;for(int fa=hT;fa!=iA;fa+=ih
)for(int c=0;c<2&&cO-c>=0;c++)if(!ck(qr->G,cO-c,fa)
&&bi<bZ){if(qr->eK[fa][cO-c])l[bi]|=(1<<gG);if(--gG<
0){gG=7;bi++;}}up=!up;}return bi+(gG<7?1:0);}static 
int er(U*l,int D,int G,char*cI,int bB){int eO=0,aZ=0
;
#define es(n)({int fH=0;for(int i=0;i<(n);i++){int \
gk=eO/8;int hF=7-(eO%8);if(gk<D){fH=(fH<<1)|((l[gk \
]>>hF)&1);}eO++;}fH;})
while(eO<D*8&&aZ<bB-1){int hZ=es(4);if(hZ==0)break;
if(hZ==4){int gL=G<=9?8:16;int F=es(gL);for(int i=0;
i<F&&aZ<bB-1;i++)cI[aZ++]=es(8);}else break;}cI[aZ]=
'\0';return aZ;}static int bj(int x1,int y1,int x2,
int y2){int dx=x2-x1,dy=y2-y1;return dx*dx+dy*dy;}
static int fQ(int x1,int y1,int x2,int y2,int x3,int
y3){return(x2-x1)*(y3-y1)-(y2-y1)*(x3-x1);}static 
int de(ac*fp,int*tl,int*tr,int*bl){int d[3]={bj(fp[0
].x,fp[0].y,fp[1].x,fp[1].y),bj(fp[0].x,fp[0].y,fp[2
].x,fp[2].y),bj(fp[1].x,fp[1].y,fp[2].x,fp[2].y)};
int eP=(d[0]>=d[1]&&d[0]>=d[2])?2:(d[1]>=d[2])?1:0;
int p1=(eP==0)?1:0,p2=(eP==2)?1:2;*tl=eP;int cp=fQ(
fp[eP].x,fp[eP].y,fp[p1].x,fp[p1].y,fp[p2].x,fp[p2].
y);*tr=(cp>0)?p1:p2;*bl=(cp>0)?p2:p1;return 1;}
static void dr(du gf[4],bF*H){float x0=gf[0].x,y0=gf
[0].y,x1=gf[1].x,y1=gf[1].y;float x2=gf[2].x,y2=gf[2
].y,x3=gf[3].x,y3=gf[3].y;float iB=x1-x3,iC=y1-y3,iD
=x2-x3,iE=y2-y3;float sx=x0-x1+x3-x2,sy=y0-y1+y3-y2;
float hU=iB*iE-iD*iC;if(fabsf(hU)<1e-10f)hU=1e-10f;
float g=(sx*iE-iD*sy)/hU;float h=(iB*sy-sx*iC)/hU;H
->h[0][0]=x1-x0+g*x1;H->h[0][1]=x2-x0+h*x2;H->h[0][2
]=x0;H->h[1][0]=y1-y0+g*y1;H->h[1][1]=y2-y0+h*y2;H->
h[1][2]=y0;H->h[2][0]=g;H->h[2][1]=h;H->h[2][2]=1.0f
;}static du eQ(const bF*H,float x,float y){du as;
float w=H->h[2][0]*x+H->h[2][1]*y+H->h[2][2];if(fabs
(w)<1e-10)w=1e-10;as.x=(H->h[0][0]*x+H->h[0][1]*y+H
->h[0][2])/w;as.y=(H->h[1][0]*x+H->h[1][1]*y+H->h[1]
[2])/w;return as;}static aJ bC(ac*fp,int tl,int tr,
int bl,int cJ){aJ O={0};float gU=fp[tr].x-fp[tl].x,
gV=fp[tr].y-fp[tl].y;float gW=fp[bl].x-fp[tl].x,gX=
fp[bl].y-fp[tl].y;float hG=sqrtf(gU*gU+gV*gV);float 
hH=sqrtf(gW*gW+gX*gX);float gm=cbrtf(fp[tl].Z*fp[tr]
.Z*fp[bl].Z);if(gm<1.0f)gm=1.0f;float gx=(hG+hH)/(
2.0f*gm);int G=cJ>0?cJ:((int)(gx+7.5f)-17+2)/4;if(G<
1)G=1;if(G>40)G=40;int aH=17+4*G;O.G=G;O.aH=aH;O.Z=
gm;float hg=aH-7;float a=gU/hg,b=gW/hg;float c=gV/hg
,d=gX/hg;float tx=fp[tl].x-3.5f*(a+b);float ty=fp[tl
].y-3.5f*(c+d);du gf[4]={{tx,ty},{a*aH+tx,c*aH+ty},{
b*aH+tx,d*aH+ty},{(a+b)*aH+tx,(c+d)*aH+ty}};dr(gf,&O
.fs);return O;}static int ar(bG*ag,const aJ*tf,int 
mx,int my,int hf){float u=(mx+0.5f)/tf->aH,v=(my+
0.5f)/tf->aH;du p=eQ(&tf->fs,u,v);if(!hf)return cZ(
ag,(int)(p.x+0.5f),(int)(p.y+0.5f))?1:0;int hI=0;for
(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){int 
ix=(int)(p.x+dx+0.5f),iy=(int)(p.y+dy+0.5f);hI+=cZ(
ag,ix,iy)*((dx|dy)?1:2);}return hI>5?1:0;}static int
cS(bG*ag,ac*fp,int iU,char*cI,int bB){if(iU<3){
return 0;}int tl,tr,bl;if(!de(fp,&tl,&tr,&bl)){
return 0;}aJ cv=bC(fp,tl,tr,bl,0);int aN=cv.G;int bQ
[10];int bX=0;bQ[bX++]=aN;float Z=cv.Z;if(aN>=25||Z<
8.0f){for(int gY=-2;gY<=2;gY++){int v=aN+gY;if(v>=1
&&v<=40&&v!=aN){bQ[bX++]=v;}}}for(int vi=0;vi<bX;vi
++){int gy=bQ[vi];aJ O=bC(fp,tl,tr,bl,gy);fE qr;qr.G
=O.G;qr.aH=O.aH;for(int my=0;my<O.aH;my++){for(int 
mx=0;mx<O.aH;mx++){qr.eK[my][mx]=ar(ag,&O,mx,my,1);}
}int am=cG(0,ag,&O);int aV=ap(am);if(aV<0){am=cG(&qr
,0,0);aV=ap(am);}if(aV<0){continue;}am=aV;int gg=(am
>>10)&0x07;int ep=(am>>13)&0x03;gT(&qr,gg);U cw[4096
];int hJ=fg(&qr,cw,sizeof(cw));U et[4096];int gZ=0,
cK=0;int D=cC(cw,hJ,O.G,ep,et,sizeof(et),&gZ,&cK);
int as=er(et,D,O.G,cI,bB);if(as>0){if(bX>1&&cK>0){
continue;}return as;}}return 0;}static int cx(bG*ag,
av*I);static void fh(av*cl){cl->I=0;cl->F=0;cl->aF=0
;}static void fi(av*cl){for(int i=0;i<cl->F;i++)free
(cl->I[i].l);free(cl->I);cl->I=0;cl->F=0;cl->aF=0;}
static int dp(av*cl,char bp,int aE,int aA,const U*l,
int D){if(cl->F>=cl->aF){int dC=cl->aF?cl->aF*2:64;
cY*fv=realloc(cl->I,dC*sizeof(cY));if(!fv)return 0;
cl->I=fv;cl->aF=dC;}cY*c=&cl->I[cl->F];c->bp=bp;c->
aE=aE;c->aA=aA;c->l=malloc(D);memcpy(c->l,l,D);c->D=
D;cl->F++;return 1;}static int eu(const char*dD,char
*bp,int*aE,int*aA,const char**cc){if(dD[0]!='N'&&dD[
0]!='P')return 0;*bp=dD[0];int n=0;if(sscanf(dD+1,
"%d/%d%n",aE,aA,&n)!=2)return 0;const char*p=dD+1+n;
while(*p==':'||*p==' ')p++;*cc=p;return 1;}static 
int fR(const void*a,const void*b){const cY*ca=a,*cb=
b;return(ca->bp!=cb->bp)?ca->bp-cb->bp:ca->aE-cb->aE
;}static void cT(av*cl){if(cl->F>1)qsort(cl->I,cl->F
,sizeof(cY),fR);if(cl->F<2)return;int w=1;for(int r=
1;r<cl->F;r++){if(cl->I[r].bp!=cl->I[w-1].bp||cl->I[
r].aE!=cl->I[w-1].aE)cl->I[w++]=cl->I[r];else free(
cl->I[r].l);}cl->F=w;}static int8_t ad[256];static 
void hs(void){static int gH=0;if(gH)return;gH=1;
memset(ad,-1,256);for(int i=0;i<26;i++){ad['A'+i]=i;
ad['a'+i]=26+i;}for(int i=0;i<10;i++)ad['0'+i]=52+i;
ad['+']=62;ad['/']=63;}static int fS(const char*ii,
int ak,U*cI,int bB){hs();int ab=0,aW=0;V ha=0;for(
int i=0;i<ak;i++){int c=(unsigned char)ii[i],fH=ad[c
];if(c=='='||c=='\n'||c=='\r'||c==' '||fH<0)continue
;ha=(ha<<6)|fH;aW+=6;if(aW>=8){aW-=8;if(ab<bB)cI[ab
++]=(ha>>aW)&0xff;}}return ab;}static int fj(av*cl,
int Y,int bq){cR();int as=0,bK=0,ba=0,an=0;int*ev=
calloc(Y+1,sizeof(int));int*ex=calloc(bq+1,sizeof(
int));int*bk=calloc(Y,sizeof(int));int*cm=calloc(bq,
sizeof(int));U**az=0,**aw=0;for(int i=0;i<cl->F;i++)
{cY*c=&cl->I[i];if(c->bp=='N'&&c->aE<=Y)ev[c->aE]=1;
else if(c->bp=='P'&&c->aE<=bq)ex[c->aE]=1;if(!ba)ba=
c->D;}for(int i=1;i<=Y;i++)if(!ev[i])bk[bK++]=i;if(
bK==0){as=1;goto gM;}for(int p=1;p<=bq&&an<bK;p++)if
(ex[p])cm[an++]=p;if(an<bK){fprintf(stderr,
"Warning: %d bK, only %d parity\n",bK,an);goto gM;}
int hK=Y-bK;int dS=an+hK;int eR=Y+an;az=malloc(dS*
sizeof(U*));aw=malloc(dS*sizeof(U*));for(int i=0;i<
dS;i++){az[i]=calloc(eR,1);aw[i]=calloc(ba,1);}for(
int i=0;i<an;i++){int ij=cm[i];for(int d=1;d<=Y;d++)
az[i][d-1]=bI[((d-1)*(ij-1))%255];az[i][Y+i]=1;}int 
fa=an;for(int i=0;i<cl->F;i++){cY*c=&cl->I[i];int cO
;if(c->bp=='N'&&c->aE<=Y)cO=c->aE-1;else if(c->bp==
'P'&&c->aE<=bq){int pi=-1;for(int j=0;j<an;j++)if(cm
[j]==c->aE){pi=j;break;}if(pi<0)continue;cO=Y+pi;}
else continue;az[fa][cO]=1;memcpy(aw[fa],c->l,ba);fa
++;}for(int cO=0;cO<eR;cO++){int cP=-1;for(int r=0;r
<dS;r++)if(az[r][cO]){cP=r;break;}if(cP<0)continue;U
iF=hR(az[cP][cO]);for(int j=0;j<eR;j++)az[cP][j]=bt(
az[cP][j],iF);for(int b=0;b<ba;b++)aw[cP][b]=bt(aw[
cP][b],iF);for(int r=0;r<dS;r++){if(r!=cP&&az[r][cO]
){U f=az[r][cO];for(int j=0;j<eR;j++)az[r][j]^=bt(f,
az[cP][j]);for(int b=0;b<ba;b++)aw[r][b]^=bt(f,aw[cP
][b]);}}}for(int i=0;i<bK;i++){int cO=bk[i]-1;for(
int r=0;r<dS;r++){if(az[r][cO]==1){int hL=1;for(int 
j=0;j<eR&&hL;j++)if(j!=cO&&az[r][j])hL=0;if(hL){dp(
cl,'N',bk[i],Y,aw[r],ba);break;}}}}as=1;gM:if(az)for
(int i=0;i<bK;i++)free(az[i]);if(aw)for(int i=0;i<bK
;i++)free(aw[i]);free(az);free(aw);free(cm);free(ev)
;free(ex);free(bk);return as;}static U*fT(av*cl,int*
ab){int aA=0,E=0;for(int i=0;i<cl->F;i++)if(cl->I[i]
.bp=='N')aA+=cl->I[i].D;if(aA==0){*ab=0;return 0;}U*
l=malloc(aA);for(int i=0;i<cl->F;i++)if(cl->I[i].bp
=='N'){memcpy(l+E,cl->I[i].l,cl->I[i].D);E+=cl->I[i]
.D;}int bb=0;V dT=0;while(bb<E&&l[bb]>='0'&&l[bb]<=
'9')dT=dT*10+(l[bb++]-'0');if(bb<E&&l[bb]==' ')bb++;
if(bb==0){*ab=E;return l;}int hh=(int)dT>E-bb?E-bb:(
int)dT;memmove(l,l+bb,hh);*ab=hh;return l;}static 
const U fk[256]={99,124,119,123,242,107,111,197,48,1
,103,43,254,215,171,118,202,130,201,125,250,89,71,
240,173,212,162,175,156,164,114,192,183,253,147,38,
54,63,247,204,52,165,229,241,113,216,49,21,4,199,35,
195,24,150,5,154,7,18,128,226,235,39,178,117,9,131,
44,26,27,110,90,160,82,59,214,179,41,227,47,132,83,
209,0,237,32,252,177,91,106,203,190,57,74,76,88,207,
208,239,170,251,67,77,51,133,69,249,2,127,80,60,159,
168,81,163,64,143,146,157,56,245,188,182,218,33,16,
255,243,210,205,12,19,236,95,151,68,23,196,167,126,
61,100,93,25,115,96,129,79,220,34,42,144,136,70,238,
184,20,222,94,11,219,224,50,58,10,73,6,36,92,194,211
,172,98,145,149,228,121,231,200,55,109,141,213,78,
169,108,86,244,234,101,122,174,8,186,120,37,46,28,
166,180,198,232,221,116,31,75,189,139,138,112,62,181
,102,72,3,246,14,97,53,87,185,134,193,29,158,225,248
,152,17,105,217,142,148,155,30,135,233,206,85,40,223
,140,161,137,13,191,230,66,104,65,153,45,15,176,84,
187,22};static const U ht[11]={0x00,0x01,0x02,0x04,
0x08,0x10,0x20,0x40,0x80,0x1b,0x36};static U df(U x)
{return(x<<1)^((x>>7)*0x1b);}static void fB(const U*
hM,U*rk,int ia){int nk=ia/4,nr=nk+6,nb=4;U*at=rk;
memcpy(at,hM,ia);U aX[4];int i=nk;while(i<nb*(nr+1))
{memcpy(aX,at+(i-1)*4,4);if(i%nk==0){U t=aX[0];aX[0]
=aX[1];aX[1]=aX[2];aX[2]=aX[3];aX[3]=t;for(int j=0;j
<4;j++)aX[j]=fk[aX[j]];aX[0]^=ht[i/nk];}else if(nk>6
&&i%nk==4)for(int j=0;j<4;j++)aX[j]=fk[aX[j]];for(
int j=0;j<4;j++)at[i*4+j]=at[(i-nk)*4+j]^aX[j];i++;}
}static void gz(const U*l,size_t au,U*cd);static U*
bR(const U*l,int au,int*fl);static void ci(V*q,const
U*l){V a,b,c,d,e,t,w[80];for(int i=0;i<16;i++){w[i]=
((V)l[i*4]<<24)|((V)l[i*4+1]<<16)|((V)l[i*4+2]<<8)|l
[i*4+3];}for(int i=16;i<80;i++){t=w[i-3]^w[i-8]^w[i-
14]^w[i-16];w[i]=(t<<1)|(t>>31);}a=q[0];b=q[1];c=q[2
];d=q[3];e=q[4];
#define gl(x,n)(((x)<<(n))|((x)>>(32-(n))))
for(int i=0;i<80;i++){V f,k;if(i<20){f=(b&c)|((~b)&d
);k=0x5a827999;}else if(i<40){f=b^c^d;k=0x6ed9eba1;}
else if(i<60){f=(b&c)|(b&d)|(c&d);k=0x8f1bbcdc;}else
{f=b^c^d;k=0xca62c1d6;}t=gl(a,5)+f+e+k+w[i];e=d;d=c;
c=gl(b,30);b=a;a=t;}q[0]+=a;q[1]+=b;q[2]+=c;q[3]+=d;
q[4]+=e;}static void ib(const U*l,size_t au,U*cd){V 
q[5]={0x67452301,0xefcdab89,0x98badcfe,0x10325476,
0xc3d2e1f0};U aO[64];size_t E=0;while(E+64<=au){ci(q
,l+E);E+=64;}size_t M=au-E;memcpy(aO,l+E,M);aO[M++]=
0x80;if(M>56){memset(aO+M,0,64-M);ci(q,aO);memset(aO
,0,56);}else{memset(aO+M,0,56-M);}uint64_t aW=au*8;
aO[56]=aW>>56;aO[57]=aW>>48;aO[58]=aW>>40;aO[59]=aW
>>32;aO[60]=aW>>24;aO[61]=aW>>16;aO[62]=aW>>8;aO[63]
=aW;ci(q,aO);for(int i=0;i<5;i++){cd[i*4]=q[i]>>24;
cd[i*4+1]=q[i]>>16;cd[i*4+2]=q[i]>>8;cd[i*4+3]=q[i];
}}static V eS(U c){return(16+(c&15))<<((c>>4)+6);}
static void fC(const char*aq,int bS,const U*gn,int 
bm,U dg,V F,U*hM,int cy){int hu=(dg==8)?32:20,E=0,bd
=0;while(E<cy){U*R=0;int ak=0;if(bm==0){ak=bd+bS;R=
malloc(ak);memset(R,0,bd);memcpy(R+bd,aq,bS);}else 
if(bm==1){ak=bd+8+bS;R=malloc(ak);memset(R,0,bd);
memcpy(R+bd,gn,8);memcpy(R+bd+8,aq,bS);}else{int fF=
8+bS;V cQ=F;if(cQ<(V)fF)cQ=fF;ak=bd+cQ;R=malloc(ak);
memset(R,0,bd);int bT=bd;while(bT<ak){int fU=fF;if(
bT+fU>ak)fU=ak-bT;if(fU<=8){memcpy(R+bT,gn,fU);}else
{memcpy(R+bT,gn,8);int dE=fU-8;if(dE>bS)dE=bS;memcpy
(R+bT+8,aq,dE);}bT+=fF;}}U cd[32];if(dg==8){gz(R,ak,
cd);}else{ib(R,ak,cd);}int dU=hu;if(E+dU>cy)dU=cy-E;
memcpy(hM+E,cd,dU);E+=dU;free(R);bd++;}}static void 
ey(const U*in,U*fl,const U*at,int nr){U q[16];memcpy
(q,in,16);for(int i=0;i<16;i++)q[i]^=at[i];for(int 
round=1;round<=nr;round++){for(int i=0;i<16;i++)q[i]
=fk[q[i]];U aX;aX=q[1];q[1]=q[5];q[5]=q[9];q[9]=q[13
];q[13]=aX;aX=q[2];q[2]=q[10];q[10]=aX;aX=q[6];q[6]=
q[14];q[14]=aX;aX=q[15];q[15]=q[11];q[11]=q[7];q[7]=
q[3];q[3]=aX;if(round<nr){for(int c=0;c<4;c++){U a[4
];for(int i=0;i<4;i++)a[i]=q[c*4+i];q[c*4+0]=df(a[0]
)^df(a[1])^a[1]^a[2]^a[3];q[c*4+1]=a[0]^df(a[1])^df(
a[2])^a[2]^a[3];q[c*4+2]=a[0]^a[1]^df(a[2])^df(a[3])
^a[3];q[c*4+3]=df(a[0])^a[0]^a[1]^a[2]^df(a[3]);}}
for(int i=0;i<16;i++)q[i]^=at[round*16+i];}memcpy(fl
,q,16);}static void fn(const U*l,int D,const U*hM,
int cy,U*cI){int nr=(cy==16)?10:(cy==24)?12:14;U at[
240];fB(hM,at,cy);U fr[16]={0},iG[16];int E=0;while(
E<D){ey(fr,iG,at,nr);int aP=16;if(E+aP>D)aP=D-E;for(
int i=0;i<aP;i++){cI[E+i]=l[E+i]^iG[i];}if(aP==16){
memcpy(fr,l+E,16);}else{memmove(fr,fr+aP,16-aP);
memcpy(fr+16-aP,l+E,aP);}E+=aP;}}static int cL(const
U*l,int au,int*al,int*ce){if(au<2)return-1;U cU=l[0]
;if((cU&0x80)==0)return-1;int ae,E=1;if(cU&0x40){ae=
cU&0x3f;if(l[E]<192){*al=l[E];E++;}else if(l[E]<224)
{if(au<E+2)return-1;*al=((l[E]-192)<<8)+l[E+1]+192;E
+=2;}else if(l[E]==255){if(au<E+5)return-1;*al=((V)l
[E+1]<<24)|((V)l[E+2]<<16)|((V)l[E+3]<<8)|l[E+4];E+=
5;}else{*al=1<<(l[E]&0x1f);E++;}}else{ae=(cU>>2)&
0x0f;int dh=cU&0x03;if(dh==0){*al=l[E];E++;}else if(
dh==1){if(au<E+2)return-1;*al=(l[E]<<8)|l[E+1];E+=2;
}else if(dh==2){if(au<E+4)return-1;*al=((V)l[E]<<24)
|((V)l[E+1]<<16)|((V)l[E+2]<<8)|l[E+3];E+=4;}else{*
al=au-E;}}*ce=E;return ae;}static int cM(const U*l,
int au){if(au<2)return 0;if((l[0]&0x80)==0)return 0;
int ae=(l[0]&0x40)?(l[0]&0x3f):((l[0]>>2)&0x0f);
return(ae==3||ae==9||ae==18);}static U*gA(const U*l,
int D,const char*aq,int*ab){*ab=0;if(!cM(l,D)){
return 0;}int E=0;U eT[32];int bn=0,bo=7;const U*bu=
0;int aG=0,fV=0;while(E<D){int al,ce;int ae=cL(l+E,D
-E,&al,&ce);if(ae<0)break;const U*eU=l+E+ce;if(ae==3
){if(al<4)return 0;int G=eU[0];if(G!=4){fprintf(
stderr,"GPG: Unsupported SKESK v%d\n",G);return 0;}
bo=eU[1];int bm=eU[2],eV=3;U dg=eU[eV++],gn[8]={0};V
F=65536;if(bm==1||bm==3){memcpy(gn,eU+eV,8);eV+=8;if
(bm==3)F=eS(eU[eV++]);}else if(bm!=0){fprintf(stderr
,"GPG: S2K bp %d\n",bm);return 0;}bn=(bo==9)?32:(bo
==8)?24:16;if(bo<7||bo>9){fprintf(stderr,
"GPG: cipher %d\n",bo);return 0;}fC(aq,strlen(aq),gn
,bm,dg,F,eT,bn);}else if(ae==9){bu=eU;aG=al;fV=0;}
else if(ae==18){if(al<1||eU[0]!=1){fprintf(stderr,
"GPG: Unsupported SEIPD G\n");return 0;}bu=eU+1;aG=
al-1;fV=1;}E+=ce+al;}if(!bu||bn==0){fprintf(stderr,
"GPG: Missing l or hM\n");return 0;}U*K=malloc(aG);
fn(bu,aG,eT,bn,K);int bD=16;if(aG<bD+2){free(K);
return 0;}if(K[bD-2]!=K[bD]||K[bD-1]!=K[bD+1]){
fprintf(stderr,
"GPG: prefix check failed (bad aq?)\n");free(K);
return 0;}int ao=bD+2;int aQ=aG-ao;if(fV){if(aQ<22){
free(K);return 0;}int fW=ao+aQ-22;if(K[fW]!=0xd3||K[
fW+1]!=0x14){fprintf(stderr,"GPG: MDC not fN\n");
free(K);return 0;}U*ez=malloc(ao+aQ-20);memcpy(ez,K,
ao+aQ-20);U dF[20];ib(ez,ao+aQ-20,dF);free(ez);if(
memcmp(dF,K+fW+2,20)!=0){fprintf(stderr,
"GPG: MDC failed\n");free(K);return 0;}aQ-=22;}int 
bU,fo,fX=cL(K+ao,aQ,&bU,&fo);U*as=0;if(fX==11){const
U*go=K+ao+fo;if(bU<6){free(K);return 0;}int gh=go[1]
,gp=2+gh+4;int cj=bU-gp;as=malloc(cj);if(as){memcpy(
as,go+gp,cj);*ab=cj;}}else if(fX==8){const U*cb=K+ao
+fo;int bV=cb[0];if(bV==0){as=malloc(bU-1);if(as){
memcpy(as,cb+1,bU-1);*ab=bU-1;}}else if(bV==1||bV==2
){int iH=(bV==2)?3:1;int iI=(bV==2)?6:0;int cV=bU-1-
iI;if(cV>0){U*dG=malloc(cV+18);memcpy(dG,
"\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff",10);
memcpy(dG+10,cb+iH,cV);memset(dG+10+cV,0,8);as=bR(dG
,cV+18,ab);free(dG);}}else{fprintf(stderr,
"GPG: compression %d\n",bV);}}else{fprintf(stderr,
"GPG: packet bp %d\n",fX);}free(K);return as;}
typedef struct{W cW;U aW;}aR;typedef struct{const U*
in;int eW;int bL;V cz;int bM;U*fl;int ab;int aZ;int 
dH;}aD;static V S(aD*s,int n){while(s->bM<n){if(s->
bL>=s->eW)return 0;s->cz|=(V)s->in[s->bL++]<<s->bM;s
->bM+=8;}V fH=s->cz&((1<<n)-1);s->cz>>=n;s->bM-=n;
return fH;}static int dI(aD*s,U b){if(s->aZ>=s->dH){
int dC=s->dH?s->dH*2:4096;s->fl=realloc(s->fl,dC);s
->dH=dC;}s->fl[s->aZ++]=b;return 1;}static void di(U
*af){for(int i=0;i<144;i++)af[i]=8;for(int i=144;i<
256;i++)af[i]=9;for(int i=256;i<280;i++)af[i]=7;for(
int i=280;i<288;i++)af[i]=8;}static int cf(const U*
af,int F,aR*fY,int*bE){int gq[16]={0};int bZ=0;for(
int i=0;i<F;i++){if(af[i]){gq[af[i]]++;if(af[i]>bZ)
bZ=af[i];}}*bE=bZ>9?9:bZ;int hN=0;int eA[16];eA[0]=0
;for(int aW=1;aW<=bZ;aW++){hN=(hN+gq[aW-1])<<1;eA[aW
]=hN;}for(int i=0;i<F;i++){int au=af[i];if(au){int c
=eA[au]++;int ik=0;for(int j=0;j<au;j++){ik=(ik<<1)|
(c&1);c>>=1;}if(au<=*bE){int iJ=1<<(*bE-au);for(int 
j=0;j<iJ;j++){int gE=ik|(j<<au);fY[gE].cW=i;fY[gE].
aW=au;}}}}return 1;}static int dj(aD*s,aR*fY,int bE)
{while(s->bM<bE){if(s->bL>=s->eW)return-1;s->cz|=(V)
s->in[s->bL++]<<s->bM;s->bM+=8;}int gE=s->cz&((1<<bE
)-1);int aW=fY[gE].aW;int cW=fY[gE].cW;s->cz>>=aW;s
->bM-=aW;return cW;}static const int hv[]={3,4,5,6,7
,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99
,115,131,163,195,227,258};static const int hb[]={0,0
,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5
,0};static const int hc[]={1,2,3,4,5,7,9,13,17,25,33
,49,65,97,129,193,257,385,513,769,1025,1537,2049,
3073,4097,6145,8193,12289,16385,24577};static const 
int gN[]={0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,
9,10,10,11,11,12,12,13,13};static const int fZ[19]={
16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
static int cg(aD*s,aR*lt,int lb,aR*dt,int db){aR*bz=
lt,*be=dt;int bW=lb,bA=db;while(1){int cW=dj(s,bz,bW
);if(cW<0)return 0;if(cW<256){if(!dI(s,cW))return 0;
}else if(cW==256){return 1;}else{cW-=257;if(cW>=29)
return 0;int au=hv[cW]+S(s,hb[cW]);int dV=dj(s,be,bA
);if(dV<0||dV>=30)return 0;int iK=hc[dV]+S(s,gN[dV])
;for(int i=0;i<au;i++){int iL=s->aZ-iK;if(iL<0)
return 0;if(!dI(s,s->fl[iL]))return 0;}}}}static int
dJ(aD*s){aR bz[512],be[32];int bW,bA;U gO[288],gr[32
];di(gO);cf(gO,288,bz,&bW);for(int i=0;i<32;i++)gr[i
]=5;cf(gr,32,be,&bA);return cg(s,bz,bW,be,bA);}
static int dk(aD*s){int hj=S(s,5)+257;int hd=S(s,5)+
1;int il=S(s,4)+4;if(hj>286||hd>30)return 0;U cX[19]
={0};for(int i=0;i<il;i++){cX[fZ[i]]=S(s,3);}aR dK[
128];int eg;if(!cf(cX,19,dK,&eg))return 0;U af[286+
30];int cn=hj+hd;int i=0;while(i<cn){int cW=dj(s,dK,
eg);if(cW<0)return 0;if(cW<16){af[i++]=cW;}else if(
cW==16){if(i==0)return 0;int eX=3+S(s,2);U iM=af[i-1
];while(eX--&&i<cn)af[i++]=iM;}else if(cW==17){int 
eX=3+S(s,3);while(eX--&&i<cn)af[i++]=0;}else if(cW==
18){int eX=11+S(s,7);while(eX--&&i<cn)af[i++]=0;}
else{return 0;}}aR bz[32768];aR be[32768];int bW,bA;
if(!cf(af,hj,bz,&bW))return 0;if(!cf(af+hj,hd,be,&bA
))return 0;return cg(s,bz,bW,be,bA);}static U*bR(
const U*l,int D,int*ab){if(D<10){*ab=0;return 0;}if(
l[0]!=0x1f||l[1]!=0x8b){*ab=0;return 0;}if(l[2]!=8){
*ab=0;return 0;}int gB=l[3];int E=10;if(gB&0x04){if(
E+2>D){*ab=0;return 0;}int iN=l[E]|(l[E+1]<<8);E+=2+
iN;}if(gB&0x08){while(E<D&&l[E])E++;E++;}if(gB&0x10)
{while(E<D&&l[E])E++;E++;}if(gB&0x02)E+=2;if(E>=D){*
ab=0;return 0;}aD s={0};s.in=l+E;s.eW=D-E-8;s.bL=0;s
.fl=0;s.aZ=0;s.dH=0;int hk;do{hk=S(&s,1);int he=S(&s
,2);if(he==0){s.cz=0;s.bM=0;if(s.bL+4>s.eW)break;int
au=s.in[s.bL]|(s.in[s.bL+1]<<8);s.bL+=4;for(int i=0;
i<au&&s.bL<s.eW;i++){if(!dI(&s,s.in[s.bL++]))break;}
}else if(he==1){if(!dJ(&s))break;}else if(he==2){if(
!dk(&s))break;}else{break;}}while(!hk);*ab=s.aZ;
return s.fl;}static const V hw[64]={0x428a2f98,
0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,
0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,
0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,
0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,
0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,
0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,
0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,
0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,
0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,
0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,
0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,
0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,
0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,
0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,
0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,
0xa4506ceb,0xbef9a3f7,0xc67178f2};
#define ei(x,n)(((x)>>(n))|((x)<<(32-(n))))
#define CH(x,y,z)(((x)&(y))^(~(x)&(z)))
#define iV(x,y,z)(((x)&(y))^((x)&(z))^((y)&(z)))
#define iW(x)(ei(x,2)^ei(x,13)^ei(x,22))
#define iX(x)(ei(x,6)^ei(x,11)^ei(x,25))
#define iO(x)(ei(x,7)^ei(x,18)^((x)>>3))
#define iP(x)(ei(x,17)^ei(x,19)^((x)>>10))
static void bN(V*q,const U*aY){V w[64];for(int i=0;i
<16;i++){w[i]=((V)aY[i*4]<<24)|((V)aY[i*4+1]<<16)|((
V)aY[i*4+2]<<8)|(V)aY[i*4+3];}for(int i=16;i<64;i++)
{w[i]=iP(w[i-2])+w[i-7]+iO(w[i-15])+w[i-16];}V a=q[0
],b=q[1],c=q[2],d=q[3];V e=q[4],f=q[5],g=q[6],h=q[7]
;for(int i=0;i<64;i++){V t1=h+iX(e)+CH(e,f,g)+hw[i]+
w[i];V t2=iW(a)+iV(a,b,c);h=g;g=f;f=e;e=d+t1;d=c;c=b
;b=a;a=t1+t2;}q[0]+=a;q[1]+=b;q[2]+=c;q[3]+=d;q[4]+=
e;q[5]+=f;q[6]+=g;q[7]+=h;}static void gz(const U*l,
size_t au,U*cd){V q[8]={0x6a09e667,0xbb67ae85,
0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,
0x1f83d9ab,0x5be0cd19};size_t i;for(i=0;i+64<=au;i+=
64){bN(q,l+i);}U aY[64];size_t M=au-i;memcpy(aY,l+i,
M);aY[M]=0x80;if(M>=56){memset(aY+M+1,0,63-M);bN(q,
aY);memset(aY,0,56);}else{memset(aY+M+1,0,55-M);}
uint64_t gP=au*8;for(int j=0;j<8;j++){aY[63-j]=gP&
0xff;gP>>=8;}bN(q,aY);for(int j=0;j<8;j++){cd[j*4]=(
q[j]>>24)&0xff;cd[j*4+1]=(q[j]>>16)&0xff;cd[j*4+2]=(
q[j]>>8)&0xff;cd[j*4+3]=q[j]&0xff;}}static int fq(ac
*p0,ac*p1,ac*p2){float ms[3]={p0->Z,p1->Z,p2->Z};
float gC=(ms[0]+ms[1]+ms[2])/3.0f;for(int i=0;i<3;i
++)if(ms[i]<gC*0.75f||ms[i]>gC*1.25f)return 0;int d[
3]={bj(p0->x,p0->y,p1->x,p1->y),bj(p0->x,p0->y,p2->x
,p2->y),bj(p1->x,p1->y,p2->x,p2->y)};if(d[0]>d[1]){
int t=d[0];d[0]=d[1];d[1]=t;}if(d[1]>d[2]){int t=d[1
];d[1]=d[2];d[2]=t;}if(d[0]>d[1]){int t=d[0];d[0]=d[
1];d[1]=t;}if(d[1]>d[0]*2)return 0;float ga=(float)d
[2]/(d[0]+d[1]);if(ga<0.75f||ga>1.25f)return 0;float
G=(sqrtf((float)d[0])/gC-10.0f)/4.0f;return G<=50;}
static int cx(bG*ag,av*I){ac ah[500];int cA=dq(ag,ah
,500);if(cA<3)return 0;int*gb=calloc(cA,sizeof(int))
;int cB=0;for(int i=0;i<cA-2&&cB<1024;i++){if(gb[i])
continue;for(int j=i+1;j<cA-1;j++){if(gb[j])continue
;for(int k=j+1;k<cA;k++){if(gb[k])continue;if(!fq(&
ah[i],&ah[j],&ah[k])){continue;}ac hV[3]={ah[i],ah[j
],ah[k]};char dL[4096];int au=cS(ag,hV,3,dL,sizeof(
dL));if(au>0){char bp;int aE,aA;const char*cc;if(eu(
dL,&bp,&aE,&aA,&cc)){U gc[4096];int eY=fS(cc,strlen(
cc),gc,sizeof(gc));if(eY>0){dp(I,bp,aE,aA,gc,eY);cB
++;gb[i]=gb[j]=gb[k]=1;goto hW;}}}}}hW:;}free(gb);
return cB;}static int gQ(int fw,char**ch,const char*
*aq,const char**aC,int*aI){for(int i=1;i<fw;i++){if(
ch[i][0]!='-'){if(!*aI)*aI=i;continue;}if(!strcmp(ch
[i],"-p")&&i+1<fw)*aq=ch[++i];else if(!strcmp(ch[i],
"-o")&&i+1<fw)*aC=ch[++i];else if(!strcmp(ch[i],"-v"
)||!strcmp(ch[i],"-vv")||!strcmp(ch[i],"--debug"))
continue;else{fprintf(stderr,
"Usage: %s [-p pass] [-o fl] ag.pbm\n",ch[0]);return
ch[i][1]=='h'?0:-1;}}if(!*aI){fprintf(stderr,
"Usage: %s [-p pass] [-o fl] ag.pbm\n",ch[0]);return
-1;}return 1;}static int gD(int fw,char**ch,int aI,
av*I){for(int i=aI;i<fw;i++){if(ch[i][0]=='-')
continue;bG*ag=fz(ch[i]);if(ag){cx(ag,I);gF(ag);}}
return I->F>0;}static U*fD(av*I,int*D){cT(I);int tn=
0,tp=0;for(int i=0;i<I->F;i++){cY*c=&I->I[i];if(c->
bp=='N'&&c->aA>tn)tn=c->aA;if(c->bp=='P'&&c->aA>tp)
tp=c->aA;}if(!fj(I,tn,tp))fprintf(stderr,
"Warning: recovery incomplete\n");return fT(I,D);}
static U*eB(U*l,int*au,const char*aq){if(!cM(l,*au))
return l;if(!aq){fprintf(stderr,
"Error: GPG encrypted, need -p\n");free(l);return 0;
}int dM;U*hO=gA(l,*au,aq,&dM);if(!hO){fprintf(stderr
,"GPG decryption failed\n");free(l);return 0;}free(l
);*au=dM;return hO;}static U*ds(U*l,int*au){if(*au<2
||l[0]!=0x1f||l[1]!=0x8b)return l;int dM;U*hO=bR(l,*
au,&dM);if(!hO)return l;free(l);*au=dM;return hO;}
static int gi(U*l,int au,const char*aC){U cd[32];gz(
l,au,cd);fprintf(stderr,"SHA256: ");for(int i=0;i<32
;i++)fprintf(stderr,"%02x",cd[i]);fprintf(stderr,
"\n");FILE*fl=aC?fopen(aC,"wb"):stdout;if(!fl)return
0;fwrite(l,1,au,fl);if(aC)fclose(fl);return 1;}int 
main(int fw,char**ch){const char*aq=0,*aC=0;int aI=0
;int r=gQ(fw,ch,&aq,&aC,&aI);if(r<=0)return r<0?1:0;
av I;fh(&I);if(!gD(fw,ch,aI,&I)){fprintf(stderr,
"No QR codes fN\n");return 1;}int au;U*l=fD(&I,&au);
fi(&I);if(!l){fprintf(stderr,
"Failed to assemble l\n");return 1;}l=eB(l,&au,aq);
if(!l)return 1;l=ds(l,&au);(void)df;int ok=gi(l,au,
aC);free(l);return ok?0:1;}
