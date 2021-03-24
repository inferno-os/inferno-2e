#----------------------------------------------------
# changjei.b
# searching for Unicode for input changjei code
#----------------------------------------------------

implement Changjei;

include "sys.m";
include "b2u.m";
include "changjei.m";

sys:Sys;
b2u:B2U;

#---------------------------------------------------------------------
# -- --   positive indicates number of unicode with same changjei code
#   |     0 no such code
#  \_/    -1 too many unicode(s) with same changjei code
#---------------------------------------------------------------------

convert_changjei_unicode(inputCode :array of int, outUnicode:array of int):int
{
   sys=load Sys Sys->PATH;
   b2u=load B2U B2U->PATH;

   
   changjeiFileDes :ref Sys->FD;
   big5FileDes: ref Sys->FD;  
 
   changjeiFileDes=sys->open(CHANG_JEI_TAB_FILE,Sys->OREAD);

   if(changjeiFileDes==nil)
   {
      sys->print("can not open"+ CHANG_JEI_TAB_FILE+"\n");
      exit;
   }


   big5FileDes=sys->open(BIG_FIVE_DAT_FILE, Sys->OREAD);
   
   encoded:=array[4] of byte;

   if(inputEncoding(inputCode, encoded)==0)
     return 0;
   fileWord:=array[NUM_OF_CHANGJEI_CHAR] of byte;

   beginNum := 16r8800;
   offset:=0;

   big5Num: int;
   hit : int;
   matchNum:=0;
   wordIndex:=0;
  
   iMiddle: int;
   cBig := array [2] of byte;

   sys->read(changjeiFileDes,fileWord,NUM_OF_CHANGJEI_CHAR);
   for(offset=0;offset<NUM_OF_CHANGJEI_CHAR;)
   {

      if( (encoded[0] ==fileWord[offset]) && (encoded[1]==fileWord[offset+1]) &&(encoded[2]==fileWord[offset+2]) )
        {
        hit=1;
        if( matchNum< (MaxSameChangjeiChar-1) )
          {
            #if((big5Num=convertWordToBig5(beginNum+wordIndex))==0)
            #       return 0;
            #sys->print("wordIndex=%d\n", wordIndex);

            sys->seek(big5FileDes, wordIndex*2,Sys->SEEKSTART);
            sys->read(big5FileDes, cBig, 2) ;           
           # sys->print("iMiddle %d\n", iMiddle);
            big5Num=int(cBig[0]);
           # sys->print("%d  %x\n", iMiddle, iMiddle);
            big5Num<<=8;
            big5Num+=int(cBig[1]);
            #sys->print("%d  %d\n", int(cBig[1]), int(cBig[1]));
            #big5Num=iMiddle;
            outUnicode[matchNum]=b2u->Big5ToUnic(big5Num);
            ++matchNum;
          }
        else
           return -1;
        }
       offset=offset+3;
       ++wordIndex;
   };

   
   if(matchNum< (MaxSameChangjeiChar-1) )
      outUnicode[matchNum]=0;
   else
      return -1;

   if(!hit)
      return 0;
   else
      return matchNum;
}

inputEncoding(inputCode: array of int, encoded: array of byte):int
{
   inputIndex: int;
 
   for(inputIndex=0; inputIndex<=4; ++inputIndex)
   {
     if( inputCode[inputIndex]==0) break;
     if((inputCode[inputIndex]>='A') && (inputCode[inputIndex]<='Z'))
        inputCode[inputIndex] =inputCode[inputIndex] - 64;
     else return 0;
   }

   
   if(inputIndex==0) return 0;
   if(inputIndex>2)
   {
      exchangeElement:=inputCode[1];
      inputCode[1]=inputCode[inputIndex-1];
      inputCode[inputIndex-1]=exchangeElement;
   }
   for(;inputIndex<=4;++inputIndex)
      inputCode[inputIndex]=0;

   sum:=0;

   for(i:=0;i<=4;++i)
   {
      bitSum:=inputCode[4-i];
      for(j:=0;j<i;++j)
      {
         bitSum=bitSum*27;
      }
      sum= sum+ bitSum;
   }

   encoded[0]= byte (sum & 16r000000FF);
   encoded[1]= byte ((sum >>8) & 16r000000FF);
   encoded[2]= byte ((sum >>16) & 16r000000FF);
   encoded[3]= byte ((sum >>24) & 16r000000FF);
   return 1;

}

convertWordToBig5 ( wordCode: int ): int
{
   big5Number: int;
   highByte    : byte;
   lowByte     : byte;

   if ( (wordCode >= 16r8001) && (wordCode <= 16r805E) )
   {
      highByte = byte ((wordCode - 16r8001) / 16r9D + 16rC6);
      lowByte  = byte ((wordCode - 16r8001) % 16r9D + 16rA1);
      big5Number   = int (highByte);
      big5Number = big5Number << 8;
      big5Number  =big5Number + ( int (lowByte));
      return big5Number;
   }

   minAreaWord : int;
   minBig5Code: int;

   if ( (wordCode >= 16r8400) && (wordCode <= 16r8597) )
   {
      minAreaWord  = 16r8400; 
      minBig5Code = 16rA1;   
   }
   else if ( (wordCode >= 16r8800) && (wordCode <= 16r9D18) )
   {
      minAreaWord  = 16r8800; 
      minBig5Code = 16rA4;  
   }
   else if ( (wordCode >= 16r9D19) && (wordCode <= 16rBB25) )
   {
      minAreaWord  = 16r9D19; 
      minBig5Code = 16rC9;  

   }
   else if ( (wordCode >= 16r805F) && (wordCode <= 16r816D) )
   {
      minAreaWord  = 16r805F;
      minBig5Code = 16rC7;  

    }
    else
      return 0;  

   highByte = byte ((wordCode - minAreaWord) / 16r9D + minBig5Code);
   lowByte  = byte ((wordCode - minAreaWord) % 16r9D);

   if ( lowByte < byte (16r3F) )
      lowByte = lowByte + byte (16r40);
   else
      lowByte = lowByte - byte (16r3F) + byte (16rA1);

   big5Number   = int (highByte);
   big5Number = big5Number << 8;
   big5Number  = big5Number + (int (lowByte));

   return big5Number;  
}

