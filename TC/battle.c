#include <graphics.h>
#include <stdio.h>
#include <string.h>

#define UP 0x11
#define DOWN 0x1f
#define LEFT 0x1e
#define RIGHT 0x20
#define KONG 0x24
#define UPT 0x48
#define DOWNT 0x50
#define LEFTT 0x4B
#define RIGHTT 0x4D
#define KONGT1 0x52
#define KONGT2 0x35
#define NEXT 0x1c
#define ESC 0x01
#define X0 139
#define Y0 39
#define X  10
#define Y  10
#define STEP  20

int load_24bit_bmp(int x, int y, char *filename);/*载入24位的bmp图片*/
void draw_stage();  /*0，空地     1，砖块    2，钢板    3，草地    4，海洋    10-11，自己坦克    12-15，敌方坦克    16-21，装备    30-45坦克在草地*/
void interrupt int_9h(void);
void move_bullets(void);
void generate_tanks(void);
void interrupt int_8h(void);
void equip_effect(int equip_num,int player);
void generate_player(int player);

struct tanks /*坦克结构*/
{
   int type;
   int x,y,d;
   int exist;
   int bullet_max;
}tank[52];

struct bullets/*子弹结构*/
{
   int owner;
   int x,y,d;
   int exist;
   int start;
}bullet[52][6];

struct picture *tankmode[6][4],*bulletmode[4];
struct picture *grassmode,*lakemode,*equipmode[6],*protectmode[2];
struct picture *starmode[2],*boommode,*blackmode[3],*whiteflagmode;
struct bullets *add_bullet(struct bullets *p,int x,int y,int d);
InterruptFunctionPointer old_8h;
InterruptFunctionPointer old_9h;
WAVE *pwave;
MIDI *pmidi;
int tank_num,tank_screen,tank_screen_max,tank_max;
int tank_style0,tank_style1,tank_style2,tank_style3;
int tank_style0_max,tank_style1_max,tank_style2_max,tank_style3_max;
int stop,press_esc,fail;
int home;
int stage;
int players;
int choose;
int map[26][26];
int map_tank[26][26];
int count=0;  /*count=1表示1/18秒*/
int c;
int equip_0t;
int equip_1t[2];
int life[2];
int equip_eat[2];
int new_life[2];
int protect[2];
int equip_clock;

void load_map(int num)
{
   FILE *fp=NULL;
   int i,j;
   char txt[10];
   char *p;
   memset(txt,0,sizeof(char)*10);
   p=(char *)malloc(sizeof("1"));
   itoa(stage, p, 10);
   strcat(txt,"map");
   strcat(txt,p);
   strcat(txt,".txt");
   if((fp=fopen(txt,"r"))==NULL)
   {
	   printf("Cannot open this file!");
	   return ;
   } 
   free(p);
   p=NULL;
   for(i=0;i<=25;i++)
      for(j=0;j<=25;j++)
	  {
         map[i][j]=fgetc(fp)-48;
		 if(map[i][j]==-38)
            map[i][j]=fgetc(fp)-48;
      }
   if(fclose(fp))
	   printf("Cannot close this file!\n");
   free(fp);
   fp=NULL;
}

void draw_brick(int x,int y,int kind)/*10*10的砖块基础*/
{
   if(kind==0)
   {
	  setfillstyle(SOLID_FILL,0x00a03000);
      bar(x, y, x+9, y+7);
	  setfillstyle(SOLID_FILL,0x00c05a00);
      bar(x, y+1, x+8, y+7);
	  setfillstyle(SOLID_FILL,0x007f7f7f);
	  bar(x, y+8, x+9, y+9);
   }
   if(kind==1)
   {
	  setfillstyle(SOLID_FILL,0x00a03000);
      bar(x, y, x+9, y+7);
	  setfillstyle(SOLID_FILL,0x00c05a00);
      bar(x+3, y+1, x+9, y+7);
	  setfillstyle(SOLID_FILL,0x007f7f7f);
	  bar(x, y, x+1, y+7);
	  bar(x, y+8, x+9, y+9);
   }
}

void draw_block(int x,int y,int kind)/*20*20的砖块基础,20*20的石头基础*/
{
   int i,j;
   if(kind==0)
      putimage(x,y,blackmode[1],COPY_PUT);
   if(kind==1)
   for(i=0;i<=1;i++)
	  for(j=0;j<=1;j++)
		 draw_brick(x+X*i,y+Y*j,(i+j)%2);
   if(kind==2)
   {
	  setfillstyle(SOLID_FILL,0x00808080);
	  bar(x, y, x+19, y+19);
	  setfillstyle(SOLID_FILL,0x00bcbcbc);
	  bar(x, y, x+19, y+1);
	  bar(x, y+2, x+17, y+3);
	  bar(x, y+4, x+1, y+19);
	  bar(x+2, y+4, x+3, y+17);
	  setfillstyle(SOLID_FILL,0x00ffffff);
	  bar(x+4, y+4, x+15, y+15);
   }
   if(kind==3 || kind/3>=10)
	   putimage(x,y,grassmode,COPY_PUT);
   if(kind==4)
	   putimage(x,y,lakemode,COPY_PUT);
}

void interrupt choose_9h(void)
{
   unsigned char key;
   key = inportb(0x60);
   if(key==ESC)
   {
	  press_esc = 1;
	  choose=1;
   }
   if( key==UP || key==DOWN || key==UPT || key==DOWNT )
   {
	  players++;
	  putimage(240, 300+(players%2)*70, blackmode[2],COPY_PUT);
	  putimage(240, 300+(1-players%2)*70, tankmode[0][3],COPY_PUT);
   }
   if( key == KONG || key == KONGT1 || key == KONGT2 )
      choose=1;
   outportb(0x61, inportb(0x61) | 0x80); /*输出字节到硬件端口中*/
   outportb(0x61, inportb(0x61) & 0x7F); /* respond to keyboard */
   outportb(0x20, 0x20); /* End Of Interrupt */
}

void draw_startmode()/*                                                      画开始界面*/
{
   PIC *p[5];/* TTF字体输出演示1: 先把多行字符串转化成多幅照片, 再输出, 速度较快 */
   long key;
   char b[14];
   setactivepage(0);
   setcolor(0xe05000); 
   p[0] = get_ttf_text_pic(" B A T T L E", "courier.ttf", 40);/* 把字符串转化成照片, 其中"courier.ttf"是TTF字库名, 40是字体大小*/
   p[1] = get_ttf_text_pic("C I T Y", "courier.ttf", 40);/* p[1]用来接收函数返回的照片指针*/
   setcolor(0xFFFAF0);
   p[2] = get_ttf_text_pic("1  PLAYER ","courier.ttf", 30);
   setcolor(0xFFFAF0);
   p[3] = get_ttf_text_pic("2  PLAYERS ","courier.ttf", 30);
   setcolor(0xFFA07A);
   p[4] = get_ttf_text_pic("CONSTRUCTION (UNFINISHED)","courier.ttf", 20);
   setcolor(0x00800080); 
   p[5] = get_ttf_text_pic("BY: Meteor","courier.ttf", 20);
   draw_picture(400-200, 100, p[0]);
   destroy_picture(p[0]);
   draw_picture(400-100, 170, p[1]);
   destroy_picture(p[1]);
   draw_picture(400-100, 300, p[2]);
   destroy_picture(p[2]);
   draw_picture(400-100, 370, p[3]);
   destroy_picture(p[3]);
   draw_picture(400-180, 470, p[4]);
   destroy_picture(p[4]);
   draw_picture(600, 550, p[5]);
   destroy_picture(p[5]);
   putimage(240, 300, tankmode[0][3],COPY_PUT);
   setcolor(0x98FB98);
   setcolor(0x191970);
   outtextxy(21,70, "instructions");
   setcolor(0xF0E68C);
   outtextxy(40,100, "player1");
   outtextxy(30,120, "w s a d  j");
   setcolor(0x00FF00);
   outtextxy(40,150, "player2");
   memset(b,0,sizeof(char)*10);
   b[0]=24;
   b[1]=' ';
   b[2]=25;
   b[3]=' ';
   b[4]=27;
   b[5]=' ';
   b[6]=26;
   b[7]=' ';
   b[8]=' ';
   b[9]='/';
   b[10]='o';
   b[11]='r';
   b[12]='0';
   b[13]='\0';
   outtextxy(20,170, b);
   setcolor(0xFF4500);
   outtextxy(50,200, "quit");
   outtextxy(52,220, "ESC");
   players=1;
   setvect(9, choose_9h);
   choose=0;
   while(!choose)
   {
     ;
   }
   setvect(9, old_9h);
   cleardevice();
}

void init_getimage(void)
{
   dword size= imagesize(0, 0, 9, 9);
   char b[20],*p,*ptr;
   int i,j;
   pwave = load_wave("start.wav"); // 载入wav文件, 返回WAVE指针
   pmidi = load_midi("bach.mid");
   setactivepage(1);
   p=(char *)malloc(sizeof("1"));
   for(i=0;i<=3;i++)/*方向*/
   {
	  memset(b,0,sizeof(char)*20);
      itoa(i, p, 10);
	  strcat(b,"0tank");
	  strcat(b,p);
	  strcat(b,".bmp");
	  load_24bit_bmp(0, 4*Y*i, b);
	  tankmode[0][i]=malloc(4*4*size);
	  getimage(0, 4*Y*i, 4*X-1, 4*Y*(i+1)-1, tankmode[0][i]);
	  memset(b,0,sizeof(char)*20);
      itoa(i, p, 10);
      strcat(b,"1tank");
      strcat(b,p);
      strcat(b,".bmp");
      load_24bit_bmp(0, 4*Y*(i+4), b);
      tankmode[1][i]=malloc(4*4*size);
      getimage(0, 4*Y*(i+4), 4*X-1, 4*Y*(i+5)-1, tankmode[1][i]);
	  memset(b,0,sizeof(char)*20);
	  strcat(b,"bullet");
	  strcat(b,p);
	  strcat(b,".bmp");
	  load_24bit_bmp(X*(4+i), 16*Y, b);
	  bulletmode[i]=malloc(size);
      getimage(X*(4+i), 16*Y, X*(i+5)-1, 17*Y-1, bulletmode[i]);
      for(j=0;j<=3;j++)
	  {
	     memset(b,0,sizeof(char)*20);
		 ptr=(char *)malloc(sizeof("1"));
		 itoa(j+2, ptr, 10);
		 strcat(b,ptr);
	     strcat(b,"tank");
	     strcat(b,p);
	     strcat(b,".bmp");
	     load_24bit_bmp(4*X*(j+1), 4*Y*i, b);
	     tankmode[j+2][i]=malloc(4*4*size);
	     getimage(4*X*(j+1), 4*Y*i, 4*X*(j+2)-1, 4*Y*(i+1)-1, tankmode[j+2][i]);
		 free(ptr);
         ptr=NULL;
	  }
   }
   for(i=0;i<=5;i++)
   {
      memset(b,0,sizeof(char)*20);
      itoa(i, p, 10);
	  strcat(b,"equip");
	  strcat(b,p);
	  strcat(b,".bmp");
	  load_24bit_bmp(4*X*(i+9), 0, b);
	  equipmode[i]=malloc(4*4*size);
	  getimage(4*X*(i+9), 0, 4*X*(i+10)-1, 4*Y-1, equipmode[i]);
   }
   for(i=0;i<=1;i++)
   {
      memset(b,0,sizeof(char)*20);
      itoa(i, p, 10);
	  strcat(b,"star");
	  strcat(b,p);
	  strcat(b,".bmp");
	  load_24bit_bmp(4*X*(i+5), 0, b);
	  starmode[i]=malloc(4*4*size);
	  getimage(4*X*(i+5), 0, 4*X*(i+6)-1, 4*Y-1, starmode[i]);
   }
   free(p);
   p=NULL;
   load_24bit_bmp(4*X*7, 0, "boom.bmp");
   boommode=malloc(4*4*size);
   getimage(4*X*7, 0, 4*X*8-1, 4*Y-1, boommode);
   blackmode[2]=malloc(4*4*size);
   getimage(4*X*8, 0, 4*X*9-1, 4*Y-1, blackmode[2]);
   load_24bit_bmp(4*X*15, 0, "grass.bmp");
   grassmode=malloc(2*2*size);
   getimage(4*X*15, 0, 4*X*15.5-1, 2*Y-1, grassmode);
   load_24bit_bmp(4*X*16, 0, "lake.bmp");
   lakemode=malloc(2*2*size);
   getimage(4*X*16, 0, 4*X*16.5-1, 2*Y-1, lakemode);
   load_24bit_bmp(4*X*17, 0, "dead.bmp");
   whiteflagmode=malloc(4*4*size);
   getimage(4*X*17, 0, 4*X*18-1, 4*Y-1, whiteflagmode);
   blackmode[1]=malloc(2*2*size);
   getimage(4*X*18, 0, 4*X*18.5-1, 2*Y-1, blackmode[1]);
   blackmode[0]=malloc(size);
   getimage(4*X*18.5, 0, 4*X*18.75-1, Y-1, blackmode[0]);
   load_24bit_bmp(4*X*17, 4*Y, "prot0.bmp");
   protectmode[0]=malloc(4*4*size);
   getimage(4*X*17, 4*Y, 4*X*18-1, 8*Y-1, protectmode[0]);
   load_24bit_bmp(4*X*17, 8*Y, "prot1.bmp");
   protectmode[1]=malloc(4*4*size);
   getimage(4*X*17, 8*Y, 4*X*18-1, 12*Y-1, protectmode[1]);
   cleardevice();
   setvisualpage(0);
}

void init_dates()
{
   int i,j;
   for(i=0;i<=25;i++)
	  for(j=0;j<=25;j++)
         map_tank[i][j]=-1;
   tank_num=0,tank_screen=0;
   tank_style0=0,tank_style1=0,tank_style2=0,tank_style3=0;
   for(i=0;i<=51;i++)
   {
	  for(j=0;j<=5;j++)
	  {
	     bullet[i][j].exist=0;
		 bullet[i][j].owner=i;
	  }
	  if(i>1)
	     tank[i].exist=0;
   }
   protect[0]=0;
   protect[1]=0;
   equip_clock=0;
   tank_style3_max=stage/5;
   tank_style2_max=stage/4;
   tank_style1_max=stage/2;
   tank_max=19+stage;
   tank_style0_max=tank_max-tank_style1_max-tank_style2_max-tank_style3_max;
   tank_screen_max=tank_max/10;
}

void generate_tanks(void)
{
   int place,i=0,j,x,y,n0,n1,n2,n3;
   char *p;
   do{
      place=random(3);
      x=place*12;
      y=0;
	  i++;
	  if(i==10)
		 return ;
   }while(map[y][x]!=0 || map[y+1][x]!=0 || map[y][x+1]!=0 || map[y+1][x+1]!=0);
   tank[tank_num+2].x=X0+4*X*place*6;
   tank[tank_num+2].y=Y0;
   n0=tank_style0,n1=tank_style1,n2=tank_style2,n3=tank_style3;
   do{
      tank[tank_num+2].type=12+random(4);
      tank_style0=n0,tank_style1=n1,tank_style2=n2,tank_style3=n3;
	  if(tank[tank_num+2].type==12)
		 tank_style0++;
	  if(tank[tank_num+2].type==13)
		 tank_style1++;
	  if(tank[tank_num+2].type==14)
		 tank_style2++;
	  if(tank[tank_num+2].type==15)
		 tank_style3++;
   }while(tank_style0>tank_style0_max || tank_style1>tank_style1_max || tank_style2>tank_style2_max || tank_style3>tank_style3_max);
   tank[tank_num+2].exist=1;
   tank[tank_num+2].d=random(4);
   tank[tank_num+2].bullet_max=tank[tank_num+2].type-11;
   for(i=0;i<=3;i++)
   {
      putimage(X0+4*X*place*6, Y0, starmode[i%2],COPY_PUT);
      c=count;
	  while(count-c<2)
	  {
	     ;
	  }
   }
   putimage(tank[tank_num+2].x, tank[tank_num+2].y, tankmode[tank[tank_num+2].type-10][tank[tank_num+2].d],COPY_PUT);
   x=(tank[tank_num+2].x-X0)/(2*X);
   y=(tank[tank_num+2].y-Y0)/(2*Y);
   map_tank[y][x]=tank_num+2;
   for(i=0;i<=1;i++)
	  for(j=0;j<=1;j++)
         map[y+i][x+j]=tank[tank_num+2].type;
   tank_num++;
   tank_screen++;
   p=malloc(sizeof("20"));
   setfillstyle(SOLID_FILL,0x7f7f7f);
   bar(780, 150, 799, 170);
   setcolor(0xFF69B4);
   itoa(tank_num-tank_screen,p, 10);
   outtextxy(780,150, p);
   bar(780, 200, 799, 220);
   setcolor(0x00BFFF);
   itoa(tank_screen,p, 10);
   outtextxy(780,200, p);
   bar(785, 250, 799, 270);
   setcolor(0xFFFAF0);
   itoa(tank_max-tank_num,p, 10);
   outtextxy(785,250, p);
   free(p);
   p=NULL;
}

void move_tanks(void)
{
   int x,y,d,move=0;
   int ii,iii,jj,i,j,n;
   for(ii=2;ii<=51;ii++)
   {
      if(tank[ii].exist==1)
      {
		  x=tank[ii].x,y=tank[ii].y;
		  i=(x-X0)/(2*X),j=(y-Y0)/(2*Y);
		  n=0;
		  if( (map[j-1][i]==0||map[j-1][i]==3||(map[j-1][i]>=16&&map[j-1][i]<=21)) && (map[j-1][i+1]==0||map[j-1][i+1]==3||(map[j-1][i+1]>=16&&map[j-1][i+1]<=21)) && (j-1>=0&&i+1<=25) )
			 n++;
		  if( (map[j+2][i]==0||map[j+2][i]==3||(map[j+2][i]>=16&&map[j+2][i]<=21)) && (map[j+2][i+1]==0||map[j+2][i+1]==3||(map[j+2][i+1]>=16&&map[j+2][i+1]<=21)) && (j+2<=25&&i+1<=25) )
			 n++;
		  if( (map[j][i-1]==0||map[j][i-1]==3||(map[j][i-1]>=16&&map[j][i-1]<=21)) && (map[j+1][i-1]==0||map[j+1][i-1]==3||(map[j+1][i-1]>=16&&map[j+1][i-1]<=21)) && (j+1<=25&&i-1>=0) )
			 n++;
		  if( (map[j][i+2]==0||map[j][i+2]==3||(map[j][i+2]>=16&&map[j][i+2]<=21)) && (map[j+1][i+2]==0||map[j+1][i+2]==3||(map[j+1][i+2]>=16&&map[j+1][i+2]<=21)) && (j+1<=25&&i+2<=25) )
			 n++;
		  if(n>=3)
			 tank[ii].d=random(4);
		  switch(tank[ii].d)
		  {
		  case 0:
			  if( (map[j-1][i]==0||map[j-1][i]==3||(map[j-1][i]>=16&&map[j-1][i]<=21)) && (map[j-1][i+1]==0||map[j-1][i+1]==3||(map[j-1][i+1]>=16&&map[j-1][i+1]<=21)) && (j-1>=0&&i+1<=25) )
				tank[ii].y -= STEP;
			 else
				while(tank[ii].d!=(d=random(4)))
				   tank[ii].d=d;
			 move=1;
			 break;
	     case 1:
		    if( (map[j+2][i]==0||map[j+2][i]==3||(map[j+2][i]>=16&&map[j+2][i]<=21)) && (map[j+2][i+1]==0||map[j+2][i+1]==3||(map[j+2][i+1]>=16&&map[j+2][i+1]<=21)) && (j+2<=25&&i+1<=25) )
			  tank[ii].y += STEP;
		    else
			  while(tank[ii].d!=(d=random(4)))
			     tank[ii].d=d;
			move=1;
		    break;
	     case 2:
			 if( (map[j][i-1]==0||map[j][i-1]==3||(map[j][i-1]>=16&&map[j][i-1]<=21)) && (map[j+1][i-1]==0||map[j+1][i-1]==3||(map[j+1][i-1]>=16&&map[j+1][i-1]<=21)) && (j+1<=25&&i-1>=0) )
				tank[ii].x -= STEP;
			 else
				while(tank[ii].d!=(d=random(4)))
				   tank[ii].d=d;
			 move=1;
			 break;
	     case 3:
			 if( (map[j][i+2]==0||map[j][i+2]==3||(map[j][i+2]>=16&&map[j][i+2]<=21)) && (map[j+1][i+2]==0||map[j+1][i+2]==3||(map[j+1][i+2]>=16&&map[j+1][i+2]<=21)) && (j+1<=25&&i+2<=25) )
				tank[ii].x += STEP;
			 else
				while(tank[ii].d!=(d=random(4)))
				   tank[ii].d=d;
			 move=1;
			 break;
	     }
		 if(move==1)
		 {
			 if( tank[ii].x < X0 )
			   tank[ii].x = X0;
			 else if( tank[ii].x + 4*X > 799 - X0 -1)
			   tank[ii].x = 799 - X0 - 4*X -1;
			 else if( tank[ii].y < Y0 )
			   tank[ii].y = Y0;
			 else if( tank[ii].y + 4*Y > 599 - Y0 -1)
			   tank[ii].y = 599 - Y0 - 4*Y -1;
			 else
			 {
			   putimage(x, y, blackmode[2],COPY_PUT);
			   map_tank[j][i]=-1;
			   for(jj=0;jj<=1;jj++)
				 for(iii=0;iii<=1;iii++)
					if(map[j+jj][i+iii]==3 || map[j+jj][i+iii]==3*tank[ii].type)
					{
					   map[j+jj][i+iii]=3;
					   draw_block(x+iii*2*X,y+jj*2*Y,map[j+jj][i+iii]);
					}   
					else
					   map[j+jj][i+iii]=0;
			   for(jj=-1;jj<=2;jj++)
				 for(iii=-1;iii<=2;iii++)
					if(map[j+jj][i+iii]>=16&&map[j+jj][i+iii]<=21)
					   putimage((i+iii)*X*2+X0,(j+jj)*Y*2+Y0, equipmode[ map[j+jj][i+iii]-16 ],COPY_PUT);
		     }
			 putimage(tank[ii].x, tank[ii].y, tankmode[tank[ii].type-10][tank[ii].d],OR_PUT);
			 x=(tank[ii].x-X0)/(2*X);
			 y=(tank[ii].y-Y0)/(2*Y);
			 map_tank[y][x]=ii;
			 for(i=0;i<=1;i++)
			   for(j=0;j<=1;j++)
				 if(map[y+i][x+j]==3)
					map[y+i][x+j]=3*tank[ii].type;
				 else
					map[y+i][x+j]=tank[ii].type;
	     }
      }
   }
}

int bullet_num(struct bullets *head)
{
   int i,sum=0;
   for(i=0;i<=5;i++)
      if(head[i].exist==1)sum++;
   return sum;
}

int main(void)
{
   int driver=0, mode=VESA_800x600x24bit;
   int i,j;
   struct bullets *ptr;
   PIC *w;
   old_8h = getvect(8);
   old_9h = getvect(9);
   initsound(); // 初始化声卡
   initgraph(&driver, &mode, "");
   randomize();
   init_getimage();
   while(!press_esc)
   {
	   setvect(9, old_9h);
	   cleardevice();
	   draw_startmode();
	   if(press_esc==1)
		  break;
	   fail=0;
	   stage=1;
       home=1;
	   new_life[0]=0;
	   new_life[1]=0;
	   tank[0].bullet_max=1;
	   tank[1].bullet_max=1;
	   life[0]=1;
	   tank[0].type=10;
       tank[1].type=11;
       tank[0].exist=1;
       if(players%2==1)
	   {
	      tank[1].exist=0;
		  life[1]=0;
	   }
       else
	   {
	      tank[1].exist=1;
		  life[1]=1;
	   }
	   while(!fail)
	   {
		   setvect(8, old_8h);
		   setvect(9, old_9h);
		   init_dates();
		   draw_stage();
		   setvect(8, int_8h);
		   setvect(9, int_9h);
		   stop=0;
		   while(!stop)/*                 当按Esc时, stop=1*/
		   {
			  if(press_esc==1)
				 break;
			  if(count%54==0 && tank_num < tank_max && tank_screen < tank_screen_max)
				 generate_tanks();
			  if(count%9==0)
			  {
				 move_tanks();
				 for(j=2;j<=51;j++)
					if(tank[j].exist==1)
					   if(bullet_num(bullet[j]) < tank[j].bullet_max)
					   {
						  ptr=add_bullet(bullet[j],tank[j].x,tank[j].y,tank[j].d);
						  for(i=0;i<=5;i++)
							 bullet[j][i]=ptr[i];
						  free(ptr);
						  ptr=NULL;
					   }
			  }
			  move_bullets();
			  if(new_life[0]==1)
                 generate_player(0);
			  if(new_life[1]==1)
                 generate_player(1);
			  if(equip_clock==1&&count-equip_0t>18*20)
			  {
				  map[25][11]=1;
				  map[24][11]=1;
				  map[23][12]=1;
				  map[23][13]=1;
				  map[23][14]=1;
				  map[24][14]=1;
				  map[25][14]=1;
				  draw_block(X0+2*X*11,Y0+2*Y*24,1);
				  draw_block(X0+2*X*11,Y0+2*Y*25,1);
				  draw_block(X0+2*X*11,Y0+2*Y*23,1);
				  draw_block(X0+2*X*12,Y0+2*Y*23,1);
				  draw_block(X0+2*X*13,Y0+2*Y*23,1);
				  draw_block(X0+2*X*14,Y0+2*Y*23,1);
				  draw_block(X0+2*X*14,Y0+2*Y*24,1);
				  draw_block(X0+2*X*14,Y0+2*Y*25,1);
			  }
			  if(protect[0]==1&&count-equip_1t[0]>18*20)
			  {
				 putimage(tank[0].x, tank[0].y, blackmode[2],COPY_PUT);
				 for(i=0;i<=1;i++)
					for(j=0;j<=1;j++)
			           draw_block(tank[0].x+2*X*i, tank[0].y+2*Y*j,map[(tank[0].y-Y0)/(2*Y)+j][(tank[0].x-X0)/(2*X)+i]);
				 putimage(tank[0].x, tank[0].y, tankmode[0][tank[0].d],OR_PUT);
			     protect[0]=0;
			  }
			  if(protect[1]==1&&count-equip_1t[1]>18*20)
			  {
			     putimage(tank[1].x, tank[1].y, blackmode[2],COPY_PUT);
				 for(i=0;i<=1;i++)
					for(j=0;j<=1;j++)
					   draw_block(tank[1].x+2*X*i, tank[1].y+2*Y*j,map[(tank[1].y-Y0)/(2*Y)+j][(tank[1].x-X0)/(2*X)+i]);
			     putimage(tank[1].x, tank[1].y, tankmode[1][tank[1].d],OR_PUT);
				 protect[1]=0;
			  }
			  if(equip_eat[0]>=16&&equip_eat[0]<=21)
			     equip_effect(equip_eat[0],0);
			  if(equip_eat[1]>=16&&equip_eat[1]<=21)
			     equip_effect(equip_eat[1],1);
			  c=count;
			  while(count-c<1)
	          {
	             ;
	          }
			  if( tank_max-tank_num==0 && tank_screen==0)
			  { 
				 setcolor(0xe05000); 
				 w = get_ttf_text_pic(" you win ","courier.ttf", 40);
				 draw_picture(400-160, 250, w);
				 destroy_picture(w);
				 c=count;
	             while(count-c<18*3)
	             {
	                ;
	             }
				 stop=1;
			  }
			  if( (tank[0].exist==0&&life[0]<=0&&tank[1].exist==0&&life[1]<=0) || home==0 )
			  { 
				 setcolor(0xe05000); 
				 w = get_ttf_text_pic(" you lost ","courier.ttf", 40);
				 draw_picture(400-160, 250, w);
				 destroy_picture(w);
				 c=count;
	             while(count-c<18*3)
	             {
	                ;
	             }
				 stop=1;
				 fail=1;
			  }
		   }
		   if(press_esc==1)
				 break;
		   stage++;
	   }
   }
   setvect(8, int_8h);
   setvect(9, old_9h);
   closegraph();
   free_wave(pwave); // 释放wav声音占用的资源
   free_midi(pmidi); // 释放midi音乐占用的资源
   closesound(); // 关闭声卡
   return 0;
}

void generate_equip(int equip_type)
{
   int x,y,map_num=equip_type+16;
   do
   {
      x=random(25);
	  y=random(24)+1;
   }
   while (map[y][x]!=0||map[y+1][x]!=0||map[y][x+1]!=0||map[y+1][x+1]!=0);
   map[y][x]=map_num;
   putimage(x*2*X+X0, y*2*Y+Y0, equipmode[equip_type],COPY_PUT);
}

void equip_rate(int type)
{
	int equip_p;
	equip_p=random(10);
	if(type==13)
	{
	  if(equip_p<1)
		 generate_equip(0);
	  else if(equip_p<2)
		 generate_equip(1);
	}
	else if(type==14)
	{
	  if(equip_p<1)
		 generate_equip(0);
	  else if(equip_p<2)
		 generate_equip(1);
	  else if(equip_p<3)
		 generate_equip(2);
	  else if(equip_p<4)
		 generate_equip(3);
	}
	else if(type==15)
	{
	  if(equip_p<1)
		 generate_equip(0);
	  else if(equip_p<2)
		 generate_equip(1);
	  else if(equip_p<3)
		 generate_equip(2);
	  else if(equip_p<4)
		 generate_equip(3);
	  else if(equip_p<5)
		 generate_equip(4);
	  else if(equip_p<6)
		 generate_equip(5);
	}
}

void del_tanks(int num)
{
   int i,j,x,y;
   tank[num].exist=0;
   putimage(tank[num].x, tank[num].y, boommode,COPY_PUT);
   c=count;
   while(count-c<2)
   {
	  ;
   }
   y=(tank[num].x-X0)/(2*X);
   x=(tank[num].y-Y0)/(2*Y);
   for(i=0;i<=1;i++)
	  for(j=0;j<=1;j++)
	  {
		 if(map[x+i][y+j]/3>=10&&map[x+i][y+j]/3<=15)
			map[x+i][y+j]=3;
		 else
			map[x+i][y+j]=0;
		 draw_block(X0+2*X*(y+j),Y0+2*Y*(x+i),map[x+i][y+j]);
	  } 
   map_tank[x][y]=-1;
}
int del_blocks(struct bullets p,int tanknum)
{
   int x,y,x1,y1,j,ii,equip_p,jj,kk;
   char *ptr;
   if(p.d==0||p.d==1)
   {
	  y=(p.x-1.5*X-X0)/(2*X);
      x=(p.y-Y0)/(2*Y);
	  if(map[x][y]==0&&map[x][y+1]==0)return 1;
	  if(map[x][y]==1||map[x][y+1]==1)/*                          打掉砖块*/
	  {
		  if(getpixel(p.x-0.1*X,p.y)==0 && getpixel(p.x-0.6*X,p.y)==0 && getpixel(p.x-1.1*X,p.y)==0 && map[x][y] < 8 )
		  {
			 if(p.d==0)
			 {
			    putimage(p.x-1.5*X,p.y-Y,blackmode[0],COPY_PUT);
			    putimage(p.x-0.5*X,p.y-Y,blackmode[0],COPY_PUT);
			 }
			 if(p.d==1)
			 {
			    putimage(p.x-1.5*X,p.y+Y,blackmode[0],COPY_PUT);
			    putimage(p.x-0.5*X,p.y+Y,blackmode[0],COPY_PUT);
			 }
			 map[x][y]=0;
		  }
		  if(getpixel(p.x+0.6*X,p.y)==0 && getpixel(p.x+1.1*X,p.y)==0 && getpixel(p.x+1.6*X,p.y)==0 && map[x][y+1] < 8 )
		  {
			 if(p.d==0)
			 {
			    putimage(p.x+0.5*X,p.y-Y,blackmode[0],COPY_PUT);
			    putimage(p.x+1.5*X,p.y-Y,blackmode[0],COPY_PUT);
			 }
			 if(p.d==1)
			 {
			    putimage(p.x+0.5*X,p.y+Y,blackmode[0],COPY_PUT);
			    putimage(p.x+1.5*X,p.y+Y,blackmode[0],COPY_PUT);
			 }
			 map[x][y+1]=0;
		  }
		  if(map[x][y]==1)
		  {
			 putimage(p.x-1.5*X,p.y,blackmode[0],COPY_PUT);
			 putimage(p.x-0.5*X,p.y,blackmode[0],COPY_PUT);
		  }
		  if(map[x][y+1]==1)
		  {
			 putimage(p.x+0.5*X,p.y,blackmode[0],COPY_PUT);
			 putimage(p.x+1.5*X,p.y,blackmode[0],COPY_PUT);
		  }
		  return 0;
	  }
	  if(map[x][y]==2||map[x][y+1]==2)
	  {
	     if(tank[tanknum].bullet_max>=4)
		 {
			 if(map[x][y]==2)
			 {
			    map[x][y]=0;
			    draw_block(X0+y*2*X,Y0+x*2*Y,map[x][y]);
			  }
			  if(map[x][y+1]==2)
			  {
				 map[x][y+1]=0;
				 draw_block(X0+(y+1)*2*X,Y0+x*2*Y,map[x][y+1]);
			  }
		 }
		 return 0;
	  } 
      if(p.owner==0 || p.owner==1)
         for(jj=-1;jj<=0;jj++)
		    for(kk=-1;kk<=1;kk++)
	           if(x+jj>=0&&x+jj<26&&y+kk>=0&&y+kk<26&&map_tank[x+jj][y+kk]>=2)
	 	       {
				   tank_screen--;
				   ptr=malloc(sizeof("20"));
				   setfillstyle(SOLID_FILL,0x7f7f7f);
				   bar(780, 150, 799, 170);
				   setcolor(0xFF69B4);
				   itoa(tank_num-tank_screen,ptr, 10);
				   outtextxy(780,150, ptr);
				   bar(780, 200, 799, 220);
				   setcolor(0x00BFFF);
				   itoa(tank_screen,ptr, 10);
				   outtextxy(780,200, ptr);
				   free(ptr);
				   ptr=NULL;
		           equip_rate(tank[map_tank[x+jj][y+kk]].type);
				   del_tanks(map_tank[x+jj][y+kk]);
				   return 0;
			   }
      if(p.owner>=2 && p.owner<=51)
         for(jj=-1;jj<=0;jj++)
		    for(kk=-1;kk<=1;kk++)
	           if(x+jj>=0&&x+jj<26&&y+kk>=0&&y+kk<26&&map_tank[x+jj][y+kk]>=0&&map_tank[x+jj][y+kk]<2&&protect[map_tank[x+jj][y+kk]]==0)
	 	       {
				   del_tanks(map_tank[x+jj][y+kk]);
				   return 0;
			   }
   }
   if(p.d==2||p.d==3)
   {
	  y=(p.x-X0)/(2*X);
      x=(p.y-1.5*Y-Y0)/(2*Y);
	  if(map[x][y]==0&&map[x+1][y]==0)return 1;
	  if(map[x][y]==1||map[x+1][y]==1)
	  {
		  if(getpixel(p.x,p.y-0.1*Y)==0 && getpixel(p.x,p.y-0.6*Y)==0 && getpixel(p.x,p.y-1.1*Y)==0 && map[x][y] < 8 )
		  {
			 if(p.d==2)
			 {
			    putimage(p.x-X,p.y-1.5*Y,blackmode[0],COPY_PUT);
			    putimage(p.x-X,p.y-0.5*Y,blackmode[0],COPY_PUT);
			 }
			 if(p.d==3)
			 {
			    putimage(p.x+X,p.y-1.5*Y,blackmode[0],COPY_PUT);
			    putimage(p.x+X,p.y-0.5*Y,blackmode[0],COPY_PUT);
			 }
			 map[x][y]=0;
		  }
		  if(getpixel(p.x,p.y+0.6*Y)==0 && getpixel(p.x,p.y+1.1*Y)==0 && getpixel(p.x,p.y+1.6*Y)==0 && map[x+1][y] < 8 )
		  {
			 if(p.d==2)
			 {
			    putimage(p.x-X,p.y+0.5*Y,blackmode[0],COPY_PUT);
			    putimage(p.x-X,p.y+1.5*Y,blackmode[0],COPY_PUT);
			 }
			 if(p.d==3)
			 {
			    putimage(p.x+X,p.y+0.5*Y,blackmode[0],COPY_PUT);
			    putimage(p.x+X,p.y+1.5*Y,blackmode[0],COPY_PUT);
			 }
			 map[x+1][y]=0;
		  }
		  if(map[x][y]==1)
		  {
			 putimage(p.x,p.y-1.5*Y,blackmode[0],COPY_PUT);
			 putimage(p.x,p.y-0.5*Y,blackmode[0],COPY_PUT);
		  }
		  if(map[x+1][y]==1)
		  {
			 putimage(p.x,p.y+0.5*Y,blackmode[0],COPY_PUT);
			 putimage(p.x,p.y+1.5*Y,blackmode[0],COPY_PUT);
		  }
		  return 0;
	  }
	  if(map[x][y]==2||map[x+1][y]==2)
	  {
	     if(tank[tanknum].bullet_max>=4)
		 {
			 if(map[x][y]==2)
			  {
				 map[x][y]=0;
				 draw_block(X0+y*2*X,Y0+x*2*Y,map[x][y]);
			  }
			  if(map[x+1][y]==2)
			  {
				 map[x+1][y]=0;
				 draw_block(X0+(y)*2*X,Y0+(x+1)*2*Y,map[x+1][y]);
			  }
		 }
		 return 0;
	  }
	  if(p.owner==0 || p.owner==1)
         for(jj=-1;jj<=1;jj++)
		    for(kk=-1;kk<=0;kk++)
	           if(x+jj>=0&&x+jj<26&&y+kk>=0&&y+kk<26&&map_tank[x+jj][y+kk]>=2)
	 	       {
				   tank_screen--;
				   ptr=malloc(sizeof("20"));
				   setfillstyle(SOLID_FILL,0x7f7f7f);
				   bar(780, 150, 799, 170);
				   setcolor(0xFF69B4);
				   itoa(tank_num-tank_screen,ptr, 10);
				   outtextxy(780,150, ptr);
				   bar(780, 200, 799, 220);
				   setcolor(0x00BFFF);
				   itoa(tank_screen,ptr, 10);
				   outtextxy(780,200, ptr);
				   free(ptr);
				   ptr=NULL;
		           equip_rate(tank[map_tank[x+jj][y+kk]].type);
				   del_tanks(map_tank[x+jj][y+kk]);
				   return 0;
			   }
      if(p.owner>=2 && p.owner<=51)
         for(jj=-1;jj<=1;jj++)
		    for(kk=-1;kk<=0;kk++)
	           if(x+jj>=0&&x+jj<26&&y+kk>=0&&y+kk<26&&map_tank[x+jj][y+kk]>=0&&map_tank[x+jj][y+kk]<2&&protect[map_tank[x+jj][y+kk]]==0)
	 	       {
				   del_tanks(map_tank[x+jj][y+kk]);
				   return 0;
			   }
   }
   return 1;
}

void move_bullets(void)
{
   struct bullets *p,*ptr;
   int x,y,i,j,ii,jj; 
   for(j=0;j<=tank_num+2;j++)
   {
	  p=bullet[j];
	  for(i=0;i<=tank[j].bullet_max;i++)
		if(p[i].exist==1)
		{
		  y=(p[i].x-X0)/(2*X);
		  x=(p[i].y-Y0)/(2*Y);
		  if(p[i].start!=1)
			 putimage(p[i].x,p[i].y,blackmode[0],COPY_PUT);
		  p[i].start=0;
		  for(jj=-1;jj<=1;jj++)
			 for(ii=-1;ii<=1;ii++)
				if((map[x+jj][y+ii]==3 || map[x+jj][y+ii]==4) && (x+jj>=0&&x+jj<=25&&y+ii>=0&&y+ii<=25))
				   draw_block((y+ii)*X*2+X0,(x+jj)*Y*2+Y0,map[x+jj][y+ii]);
		  for(ii=-1;ii<=1;ii++)
			 for(jj=-1;jj<=1;jj++)
				if(map[x+ii][y+jj]>=16&&map[x+ii][y+jj]<=21)
				   putimage((y+jj)*X*2+X0,(x+ii)*Y*2+Y0, equipmode[ map[x+ii][y+jj]-16 ],COPY_PUT);
		  if(p[i].d==0)p[i].y -= Y;
		  else if(p[i].d==1)p[i].y += Y;
		  else if(p[i].d==2)p[i].x -= X;
		  else if(p[i].d==3)p[i].x += X;
		  if(p[i].x>=X0+X*4*6 && p[i].x<X0+X*4*7 && p[i].y>=Y0+Y*4*12 && p[i].y< Y0+Y*4*13)/*如果打到老家*/
		  {
			 putimage(X0+X*4*6, Y0+Y*4*12, boommode,COPY_PUT);
			 c=count;
		     while(count-c<2)
		     {
			    ;
		     }
			 putimage(X0+X*4*6, Y0+Y*4*12, whiteflagmode,COPY_PUT);
			 p[i].exist=0;
			 home=0;
		  }
		  else if(p[i].x<X0 || p[i].x>799-X0-2 || p[i].y<Y0 || p[i].y>599-Y0-2)/*如果打出边界*/
			 p[i].exist=0;
		  else if((p[i].exist=del_blocks(p[i],j))==1)
			 putimage(p[i].x, p[i].y, bulletmode[p[i].d],OR_PUT);
		}
   }
   free(p);
   p=NULL;
   free(ptr);
   ptr=NULL;
}

struct bullets *add_bullet(struct bullets *p,int x,int y,int d)
{
   int i;
   for(i=0;i<=5;i++)
      if(p[i].exist==0)
	  {
	     p[i].exist=1;
		 p[i].d=d;
         if(p[i].d==0){p[i].x=x+1.5*X;p[i].y=y;}
	     if(p[i].d==1){p[i].x=x+1.5*X;p[i].y=y+3*Y;}
	     if(p[i].d==2){p[i].x=x;p[i].y=y+1.5*Y;}
	     if(p[i].d==3){p[i].x=x+3*X;p[i].y=y+1.5*Y;}
		 p[i].start=1;
		 break;
	   }
   return p;
}

void equip_effect(int equip_num,int player)
{
   int i,j,ii,x1,y1;
   char *p;
   int cc;
   equip_num=equip_num-16;
   if(equip_num==0)
   {
      equip_0t=count;
	  map[24][11]=2;
	  map[25][11]=2;
	  map[23][11]=2;
	  map[23][12]=2;
	  map[23][13]=2;
	  map[23][14]=2;
	  map[24][14]=2;
	  map[25][14]=2;
	  draw_block(X0+2*X*11,Y0+2*Y*24,2);
	  draw_block(X0+2*X*11,Y0+2*Y*25,2);
	  draw_block(X0+2*X*11,Y0+2*Y*23,2);
	  draw_block(X0+2*X*12,Y0+2*Y*23,2);
	  draw_block(X0+2*X*13,Y0+2*Y*23,2);
	  draw_block(X0+2*X*14,Y0+2*Y*23,2);
	  draw_block(X0+2*X*14,Y0+2*Y*24,2);
	  draw_block(X0+2*X*14,Y0+2*Y*25,2);
   }
   if(equip_num==1)
   {
      protect[player]=1;
	  equip_1t[player]=count;
   }
   if(equip_num==2)
   {
      c=count;
	  while(count-c<18*10)
	  {
		 move_bullets();
		 cc=count;
		 while(count-cc<1)
		 {
		    ;
		 }
	  }
   }
   if(equip_num==3)
	   for(i=2;i<=tank_num+2;i++)
			if( tank[i].exist==1 )
			{
			   tank[i].exist=0;
			   putimage(tank[i].x, tank[i].y, boommode,COPY_PUT);
			   tank_screen--;
			   p=malloc(sizeof("20"));
			   setfillstyle(SOLID_FILL,0x7f7f7f);
			   bar(780, 150, 799, 170);
			   setcolor(0xFF69B4);
			   itoa(tank_num-tank_screen,p, 10);
			   outtextxy(780,150, p);
			   bar(780, 200, 799, 220);
			   setcolor(0x00BFFF);
			   itoa(tank_screen,p, 10);
			   outtextxy(780,200, p);
			   free(p);
			   p=NULL;
			   c=count;
			   while(count-c<2)
			   {
				  ;
			   }
			   y1=(tank[i].x-X0)/(2*X);
		       x1=(tank[i].y-Y0)/(2*Y);
		       for(ii=0;ii<=1;ii++)
			   for(j=0;j<=1;j++)
			   {
			     if(map[x1+ii][y1+j]/3>=12&&map[x1+ii][y1+j]/3<=15)
				    map[x1+ii][y1+j]=3;
			     else
				    map[x1+ii][y1+j]=0;
		         draw_block(tank[i].x+2*X*ii,tank[i].y+2*Y*j,map[x1+ii][y1+j]);
		       } 
			}
   if(equip_num==4)
	{
	  if(tank[player].bullet_max<6)
	  {
		  tank[player].bullet_max=tank[player].bullet_max+1;
		  bar(95+player*(765-95),420, 30+95+player*(760-90),440);
		  setcolor(0xFFA07A);
		  p=malloc(sizeof("1234"));
		  itoa(tank[player].bullet_max,p, 10);
		  outtextxy(95+player*(765-95),420, p);
		  free(p);
		  p=NULL;
	  }
   }
   if(equip_num==5)
   {
	   setfillstyle(SOLID_FILL,0x007f7f7f);
	   bar(90+player*(760-90),400, 30+90+player*(760-90),420);
	   setcolor(0xFFA07A);
	   life[player]=life[player]+1;
	   p=malloc(sizeof("1"));
	   itoa(life[player],p, 10);
	   outtextxy(90+player*(760-90),400, p);
	   free(p);
	   p=NULL;
   }
   equip_eat[player]=-1;
}

void interrupt int_8h(void)
{
   count++;
   if(tank[0].exist==1&&protect[0]==1)
      putimage(tank[0].x, tank[0].y, protectmode[count%2],OR_PUT);
   if(tank[1].exist==1&protect[1]==1)
         putimage(tank[1].x, tank[1].y, protectmode[count%2],OR_PUT);
   outportb(0x20, 0x20);
}

void interrupt int_9h(void)
{
   unsigned char key;
   int x,y,move;
   int i,j,ii,jj;
   struct bullets *p;
   key = inportb(0x60);  /* read key code */
   switch(key)
   {
	  case ESC:
		press_esc = 1;
		stop = 1;
		fail = 1;
		break;
	  case NEXT:
		stop = 1;
		break;
   }
   if(tank[0].exist==1)
   {
	   x=tank[0].x,y=tank[0].y;
	   i=(x-X0)/(2*X),j=(y-Y0)/(2*Y);
	   move=0;
	   switch(key)
	   {
	   case UP:
		  move=1;
		  if(tank[0].d==0)
			  if( (map[j-1][i]==0||map[j-1][i]==3||(map[j-1][i]>=16&&map[j-1][i]<=21)) && (map[j-1][i+1]==0||map[j-1][i+1]==3||(map[j-1][i+1]>=16&&map[j-1][i+1]<=21)) && (j>=0&&j+1<=25&&i>=0&&i+1<=25) )
				 tank[0].y -= STEP;
	          else
				 move=0;
		  tank[0].d=0;
		  break;
	   case DOWN:
		  move=1;
		  if(tank[0].d==1)
			 if( (map[j+2][i]==0||map[j+2][i]==3||(map[j+2][i]>=16&&map[j+2][i]<=21)) && (map[j+2][i+1]==0||map[j+2][i+1]==3||(map[j+2][i+1]>=16&&map[j+2][i+1]<=21)) && (j>=0&&j+1<=25&&i>=0&&i+1<=25) )
				tank[0].y += STEP;
		     else
				 move=0;
		  tank[0].d=1;
		  break;
	   case LEFT:
		  move=1;
		  if(tank[0].d==2)
			 if( (map[j][i-1]==0||map[j][i-1]==3||(map[j][i-1]>=16&&map[j][i-1]<=21)) && (map[j+1][i-1]==0||map[j+1][i-1]==3||(map[j+1][i-1]>=16&&map[j+1][i-1]<=21)) && (j>=0&&j+1<=25&&i>=0&&i+1<=25) )
				tank[0].x -= STEP;
		     else
				 move=0;
		  tank[0].d=2;
		  break;
	   case RIGHT:
		  move=1;
		  if(tank[0].d==3)
			 if( (map[j][i+2]==0||map[j][i+2]==3||(map[j][i+2]>=16&&map[j][i+2]<=21)) && (map[j+1][i+2]==0||map[j+1][i+2]==3||(map[j+1][i+2]>=16&&map[j+1][i+2]<=21)) && (j>=0&&j+1<=25&&i>=0&&i+1<=25) )
				tank[0].x += STEP;
		     else
				 move=0;
		  tank[0].d=3;
		  break;
	   case KONG:
		  if(bullet_num(bullet[0]) < tank[0].bullet_max)
		  {
			 p=add_bullet(bullet[0],x,y,tank[0].d);
			 for(ii=0;ii<=5;ii++)
				 bullet[0][ii]=p[ii];
		  }
		  break;
	   }
	   if(move==1)
	   {
		   if( tank[0].x < X0 )
			  tank[0].x = X0;
		   else if( tank[0].x + 4*X > 799 - X0 -1)
			  tank[0].x = 799 - X0 - 4*X -1;
		   else if( tank[0].y < Y0 )
			  tank[0].y = Y0;
		   else if( tank[0].y + 4*Y > 599 - Y0 -1)
			  tank[0].y = 599 - Y0 - 4*Y -1;
		   else
		   {
			  putimage(x, y, blackmode[2],COPY_PUT);
			  map_tank[j][i]=-1;
			  for(jj=0;jj<=1;jj++)
				 for(ii=0;ii<=1;ii++)
					if(map[j+jj][i+ii]==3 || map[j+jj][i+ii]==3*tank[0].type)
					{
					   map[j+jj][i+ii]=3;
					   draw_block(x+ii*2*X,y+jj*2*Y,map[j+jj][i+ii]);
					}
					else
					   map[j+jj][i+ii]=0;
			  for(ii=-1;ii<=2;ii++)
				 for(jj=-1;jj<=2;jj++)
					if(map[j+jj][i+ii]>=16&&map[j+jj][i+ii]<=21)
					{
					   putimage((i+ii)*X*2+X0,(j+jj)*Y*2+Y0, blackmode[2],COPY_PUT);
					   equip_eat[0]=map[j+jj][i+ii];
					   map[j+jj][i+ii]=0;
					}
			  putimage(tank[0].x, tank[0].y, tankmode[0][tank[0].d],OR_PUT);
			  x=(tank[0].x-X0)/(2*X);
			  y=(tank[0].y-Y0)/(2*Y);
			  map_tank[y][x]=0;
			  for(i=0;i<=1;i++)
				  for(j=0;j<=1;j++)
					 if(map[y+i][x+j]==3 )
						map[y+i][x+j]=3*tank[0].type;
					 else
						map[y+i][x+j]=tank[0].type;
		   }
	   }
   }
   else if(key==KONG&&life[0]>=1)
   {
	  tank[0].exist=1;
	  new_life[0]=1;
	  life[0]--;
   }
   if(tank[1].exist==1)
   {
	   x=tank[1].x,y=tank[1].y;
	   i=(x-X0)/(2*X),j=(y-Y0)/(2*Y);
	   move=0;
	   switch(key)
	   {
	   case UPT:
		  move=1;
		  if(tank[1].d==0)
			 if( (map[j-1][i]==0||map[j-1][i]==3||(map[j-1][i]>=16&&map[j-1][i]<=21)) && (map[j-1][i+1]==0||map[j-1][i+1]==3||(map[j-1][i+1]>=16&&map[j-1][i+1]<=21)) && (j>=0&&j+1<=25&&i>=0&&i+1<=25) )
			    tank[1].y -= STEP;
		     else
			    move=0;
		  tank[1].d=0;
		  break;
	   case DOWNT:
		  move=1;
		  if(tank[1].d==1)
			 if( (map[j+2][i]==0||map[j+2][i]==3||(map[j+2][i]>=16&&map[j+2][i]<=21)) && (map[j+2][i+1]==0||map[j+2][i+1]==3||(map[j+2][i+1]>=16&&map[j+2][i+1]<=21)) && (j>=0&&j+1<=25&&i>=0&&i+1<=25) )
				tank[1].y += STEP;
		     else
				 move=0;
		  tank[1].d=1;
		  break;
	   case LEFTT:
		  move=1;
		  if(tank[1].d==2)
			 if( (map[j][i-1]==0||map[j][i-1]==3||(map[j][i-1]>=16&&map[j][i-1]<=21)) && (map[j+1][i-1]==0||map[j+1][i-1]==3||(map[j+1][i-1]>=16&&map[j+1][i-1]<=21)) && (j>=0&&j+1<=25&&i>=0&&i+1<=25) )
				tank[1].x -= STEP;
		     else
				 move=0;
		  tank[1].d=2;
		  break;
	   case RIGHTT:
		  move=1;
		  if(tank[1].d==3)
			 if( (map[j][i+2]==0||map[j][i+2]==3||(map[j][i+2]>=16&&map[j][i+2]<=21)) && (map[j+1][i+2]==0||map[j+1][i+2]==3||(map[j+1][i+2]>=16&&map[j+1][i+2]<=21)) && (j>=0&&j+1<=25&&i>=0&&i+1<=25) )
				tank[1].x += STEP;
             else
				 move=0;
		  tank[1].d=3;
		  break;
	   case KONGT1:
	   case KONGT2:
		  if(bullet_num(bullet[1]) < tank[1].bullet_max)
		  {
			 p=add_bullet(bullet[1],x,y,tank[1].d);
			 for(ii=0;ii<=5;ii++)
				 bullet[1][ii]=p[ii];
		  }
		  break;
	   }
	   if(move==1)
	   {
		   if( tank[1].x < X0 )
			  tank[1].x = X0;
		   else if( tank[1].x + 4*X > 799 - X0 -1)
			  tank[1].x = 799 - X0 - 4*X -1;
		   else if( tank[1].y < Y0 )
			  tank[1].y = Y0;
		   else if( tank[1].y + 4*Y > 599 - Y0 -1)
			  tank[1].y = 599 - Y0 - 4*Y -1;
		   else
		   {
			  putimage(x, y, blackmode[2],COPY_PUT);
			  map_tank[j][i]=-1;
			  for(jj=0;jj<=1;jj++)
				 for(ii=0;ii<=1;ii++)
					if(map[j+jj][i+ii]==3 || map[j+jj][i+ii]==3*tank[1].type)
					{
					   map[j+jj][i+ii]=3;
					   draw_block(x+ii*2*X,y+jj*2*Y,map[j+jj][i+ii]);
					}
					else
					   map[j+jj][i+ii]=0;
			  for(ii=-1;ii<=2;ii++)
				 for(jj=-1;jj<=2;jj++)
					if(map[j+jj][i+ii]>=16&&map[j+jj][i+ii]<=21)
					{
					   putimage((i+ii)*X*2+X0,(j+jj)*Y*2+Y0, blackmode[2],COPY_PUT);
					   equip_eat[1]=map[j+jj][i+ii];
					   map[j+jj][i+ii]=0;
					}
			  putimage(tank[1].x, tank[1].y, tankmode[1][tank[1].d],OR_PUT);
			  x=(tank[1].x-X0)/(2*X);
			  y=(tank[1].y-Y0)/(2*Y);
			  map_tank[y][x]=1;
			  for(i=0;i<=1;i++)
				 for(j=0;j<=1;j++)
					 if(map[y+i][x+j]==3 )
						map[y+i][x+j]=3*tank[1].type;
					 else
						map[y+i][x+j]=tank[1].type;
		   }
	   }
   }
   else if((key==KONGT1||key==KONGT2)&&life[1]>=1)
   {
	  tank[1].exist=1;
	  new_life[1]=1;
	  life[1]--;
   }
   free(p);
   p=NULL;
   outportb(0x61, inportb(0x61) | 0x80); /*输出字节到硬件端口中*/
   outportb(0x61, inportb(0x61) & 0x7F); /* respond to keyboard */
   outportb(0x20, 0x20); /* End Of Interrupt */
}

void generate_player(int player)
{
   int i,j,x,y;
   char *p;
   tank[player].x=X0+X*4*(4.5+player*3);
   tank[player].y=Y0+Y*4*12;
   tank[player].d=0;
   setfillstyle(SOLID_FILL,0x007f7f7f);
   bar(20+player*(690-20),400, 30+90+player*(760-90),420);
   if(player==0)
   {
	  setcolor(0xF0E68C);
      outtextxy(20,400, "1P life: ");
   }
   if(player==1)
   {
	  setcolor(0x00FF00);
      outtextxy(690,400, "2P life: ");
   }
   p=malloc(sizeof("1"));
   itoa(life[player],p, 10);
   outtextxy(90+player*(760-90),400, p);
   if(player==0)
   {
      setcolor(0xF0E68C);
	  outtextxy(20,420, "1P level: ");
   }
   if(player==1)
   {
      setcolor(0x00FF00);
	  outtextxy(690,420, "2P level: ");
   }
   itoa(tank[player].bullet_max,p, 10);
   outtextxy(95+player*(765-95),420, p);
   free(p);
   p=NULL;
   putimage(tank[player].x, tank[player].y, tankmode[player][0],COPY_PUT);
   x=(tank[player].x-X0)/(2*X);
   y=(tank[player].y-Y0)/(2*Y);
   map_tank[y][x]=player;
   for(i=0;i<=1;i++)
	  for(j=0;j<=1;j++)
		 map[y+i][x+j]=10+player;
   new_life[player]=0;
   protect[player]=1;
   equip_1t[player]=count;
}

void draw_stage()/*1，砖块    2，钢板    3，草地    4，海洋*/
{
   int i,j,x,y;
   char *p,b[14];
   play_midi(pmidi, -1);
   setactivepage(1);
   cleardevice();
   setfillstyle(SOLID_FILL,0x7f7f7f);
   bar(0, 0, 799, 599);
   memset(b,0,sizeof(char)*10);
   p=malloc(sizeof("1"));
   itoa(stage,p, 10);
   strcat(b,"stage  ");
   strcat(b,p);
   setcolor(0x00000000); 
   outtextxy(370,300, b);
   setvisualpage(1);
   setactivepage(0);
   cleardevice();
   load_map(1);
   setfillstyle(SOLID_FILL,0x7f7f7f);
   bar(0, 0, 799, 599);
   setfillstyle(SOLID_FILL,0x000000);
   bar(X0, Y0, 799-X0-2, 599-Y0-2);
   for(i=0;i<=25;i++)
	   for(j=0;j<=25;j++)
          draw_block(X0+X*2*i,Y0+Y*2*j,map[j][i]);
   load_24bit_bmp(X0+X*4*6, Y0+Y*4*12, "eagle.bmp");
   setcolor(0xFFA07A);
   outtextxy(697,50, b);
   setcolor(0x191970);
   outtextxy(21,70, "instructions");
   setcolor(0xF0E68C);
   outtextxy(40,100, "player1");
   outtextxy(30,120, "w s a d  j");
   setcolor(0x00FF00);
   outtextxy(40,150, "player2");
   memset(b,0,sizeof(char)*10);
   b[0]=24;
   b[1]=' ';
   b[2]=25;
   b[3]=' ';
   b[4]=27;
   b[5]=' ';
   b[6]=26;
   b[7]=' ';
   b[8]=' ';
   b[9]='/';
   b[10]='o';
   b[11]='r';
   b[12]='0';
   b[13]='\0';
   outtextxy(20,170, b);
   setcolor(0xFF4500);
   outtextxy(50,200, "quit");
   outtextxy(52,220, "ESC");
   setcolor(0x556B2F);
   outtextxy(30,250, "next stage");
   outtextxy(48,270, "ENTER");
   setcolor(0x98FB98);
   outtextxy(670,100, "totale tanks: ");
   itoa(tank_max,p, 10);
   outtextxy(780,100, p);
   setcolor(0xFF69B4);
   outtextxy(680,150, "dead tanks: ");
   itoa(tank_num-tank_screen,p, 10);
   outtextxy(780,150, p);
   setcolor(0x00BFFF);
   outtextxy(670,200, "screen tanks: ");
   itoa(tank_screen,p, 10);
   outtextxy(780,200, p);
   setcolor(0xFFFAF0);
   outtextxy(660,250, "remaining tanks: ");
   itoa(tank_max-tank_num ,p, 10);
   outtextxy(785,250, p);
   setcolor(0x00800080);
   outtextxy(695,500, "BY: Meteor");
   if(tank[0].exist==1||life[0]>0)
	   generate_player(0);
   if(players%2==0&&(tank[1].exist==1||life[1]>0))
       generate_player(1);
   getch();
   setvisualpage(0);
   free(p);
   p=NULL;
}

int load_24bit_bmp(int x, int y, char *filename)
{
   FILE *fp = NULL;
   byte *p = NULL; /* pointer to a line of bmp data */
   byte *vp = _vp + _active_page*_page_gap + (y*_width + x) * (_color_bits/8);
   dword width, height, bmp_data_offset, bytes_per_line, offset;
   int i;
   p = malloc(800L * 3);  /* 分配能够存储一行bmp文件数据的指针 */
   if(p == NULL)  /* 内存太少，不能够画一条线 */
      goto display_bmp_error;
   fp = fopen(filename, "rb");
   if(fp == NULL) /* 不能打开bmp文件 */
      goto display_bmp_error;
   fread(p, 1, 0x36, fp);     /* 读取bmp文件数据头指针 0X36=54 除了文件的位图信息和调色板内容以外的全部信息*/
   if(*(word *)p != 0x4D42)   /* 不是bmp文件  4D:M  42:B*/
      goto display_bmp_error; 
   if(*(word *)(p+0x1C) != 24)/* 字节 #28-29 保存每个像素的位数，它是图像的颜色深度。常用值是1、4、8（灰阶）和24（彩色）*/
      goto display_bmp_error; 
   width = *(dword *)(p+0x12);/*字节 #18-21 保存位图宽度（以像素个数表示）*/
   height = *(dword *)(p+0x16);/*字节 #22-25 保存位图高度（以像素个数表示）*/
   bmp_data_offset = *(dword *)(p+0x0A);/*字节 #10-13 保存位图数据位置的地址偏移，也就是起始地址*/
   fseek(fp, bmp_data_offset, SEEK_SET); /* 读取bmp文件位图信息头指针 */
   bytes_per_line = (width * 3 + 3) / 4 * 4; /* 必须是4的倍数 */
   for(i=height-1; i>=0; i--)          /* 从底部开始往上画 */
   {
      fread(p, 1, bytes_per_line, fp); /* 读取一行数据 */
      offset = i * 800 * 3;
      memcpy(vp+offset, p, width*3);
   }
   free(p);
   fclose(fp);
   return 1;
display_bmp_error:
   if(p != NULL)
      free(p);
   if(fp != NULL)
      fclose(fp);
   return 0;
}
