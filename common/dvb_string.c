#include <ctype.h>
#include <stdlib.h>
#include <assert.h>
#include <iconv.h>
#include <string.h>
#include <errno.h>
#include <locale.h>
#include <wchar.h>
#include "in_out.h"
#include "ipc.h"
#include "dvb_string.h"
#include "utl.h"
#include "debug.h"
#include "dmalloc_loc.h"
/*
const uint8_t CC_EMPH_ON[]={0x86};
const uint8_t CC_EMPH_ON_UTF8[]={0xC2,0x86};
const uint8_t CC_EMPH_OFF[]={0x87};
const uint8_t CC_EMPH_OFF_UTF8[]={0xC2,0x87};
const uint8_t CC_CRLF[]={0x8A};
const uint8_t CC_CRLF_UTF8[]={0xC2,0x8A};

const uint16_t CC_EMPH_ON_2B[]={0xE086};
const uint8_t CC_EMPH_ON_2B_UTF8[]={0xEE,0x82,0x86};
const uint16_t CC_EMPH_OFF_2B[]={0xE087};
const uint8_t CC_EMPH_OFF_2B_UTF8[]={0xEE,0x82,0x87};
const uint16_t CC_CRLF_2B[]={0xE08A};
const uint8_t CC_CRLF_2B_UTF8[]={0xEE,0x82,0x8A};

conversion still messed up..
counted space too small because the dvb codepts(0x8a).. 
are still in there and get conv wrongly..
how to fix those up...
have to replace before converting...
better remove all chars which are not in codeset..
(it's input from outside world after all)

rough outline:

convert some sequences on input depending on charset..

utf8bitseqs        8bitseqs

UTF-8              ISO-8859-x
                   ASCII
                   ISO-6937

16 bit seqs
"ISO-10646/UCS2"
"EUC-KR"
"GB2312"
"BIG5"
x€x
that euro is...
e2 82 ac in utf8..

the ISO-6937 has a euro symbol at pos 0xA4
howto replace? may only be possible after conv...
what about the black squares at: a6,c0,c9,cc,d8-db,e5?
are all control codes valid?(I know ncurses can not do all of them,
and even if it did, I might not want it...)
Not even sure I wnat this ncurses-specific...


at least the 8859..tables seem pretty generic...

*/
void
dvb_string_ascii_sanitize (char *p)
{
  if (p == NULL)
    return;
  while (*p)
  {
    if (*p == '\x8A')           //CR/LF in dvb terms
      *p = '\n';                //newline
    else if ((*p == '\x86') || (*p == '\x87'))
      *p = '*';
    else if (!isascii (*p))
      *p = ' ';
    p++;
  }
}

/*
replace emph with '*' dvb-newlines with proper newlines
single byte codez
*/
void
dvb_string_repl1b (char *p, int sz)
{
  while (sz)
  {
    if (*p == '\x8A')           //CR/LF in dvb terms
      *p = '\n';                //newline
    else if ((*p == '\x86') || (*p == '\x87'))
      *p = '*';
    p++;
    sz--;
  }
}

//2byte codes. size is byte count
void
dvb_string_repl2b (uint16_t * p, int sz)
{
  while (sz)
  {
    if (*p == 0xE08A)           //CR/LF in dvb terms
      *p = '\n';                //newline
    else if ((*p == 0xE086) || (*p == 0xE087))  //emphasis
      *p = '*';
    p++;
    sz -= 2;
  }
}

//utf8 codes
/*
const uint8_t CC_EMPH_ON_UTF8[]={0xC2,0x86};
const uint8_t CC_EMPH_OFF_UTF8[]={0xC2,0x87};
const uint8_t CC_CRLF_UTF8[]={0xC2,0x8A};

const uint8_t CC_EMPH_ON_2B_UTF8[]={0xEE,0x82,0x86};
const uint8_t CC_EMPH_OFF_2B_UTF8[]={0xEE,0x82,0x87};
const uint8_t CC_CRLF_2B_UTF8[]={0xEE,0x82,0x8A};
*/

/*remove one char by memmoving rest of buffer forward
\param sz size of whole buffer
\param p points to start of buffer
\param idx byte to remove/overwrite*/
static void
remove_char (char *p, int idx, int sz)
{
  if (idx > (sz + 1))
    memmove (p + idx, p + idx + 1, sz - idx - 1);
}

//deletes /backwards/
static void
remove_2char (char *p, int idx, int sz)
{
  remove_char (p, idx, sz);
  sz--;
  idx--;
  remove_char (p, idx, sz);
}


void
dvb_string_replu8 (char **p_r, int *sz_r)
{
#define DO_RET *p_r=p;*sz_r=sz;return
  char *p = *p_r;
  int sz = *sz_r;
  int i;
  for (i = 0; i < sz; i++)
  {
    if ((p[i] & 0xfc) == 0xfc)
    {                           //6 bytes
      i += 5;
    }
    else if ((p[i] & 0xf8) == 0xf8)
    {                           //5 bytes
      i += 4;
    }
    else if ((p[i] & 0xf0) == 0xf0)
    {                           //4 bytes
      i += 3;
    }
    else if ((p[i] & 0xe0) == 0xe0)
    {                           //3 bytes
      if (p[i] == '\xEE')
      {
        i++;
        if (i >= sz)
        {
          p[i - 1] = ' ';
          DO_RET;
        }
        if (p[i] == '\x82')
        {
          i++;
          if (i >= sz)
          {
            p[i - 1] = ' ';
            p[i - 2] = ' ';
            DO_RET;
          }
          switch (p[i])
          {
          case '\x86':
          case '\x87':
            remove_2char (p, i, sz);
            sz -= 2;
            i -= 2;
            p[i] = '*';
            break;
          case '\x8A':
            remove_2char (p, i, sz);
            sz -= 2;
            i -= 2;
            p[i] = '\n';
            break;
          default:
            break;
          }
        }
        else
          i++;                  //coz it's supposed to b a 3 byte code
      }
      else
        i += 2;                 //ditto
    }
    else if ((p[i] & 0xc0) == 0xc0)
    {                           //2 bytes
      if (p[i] == '\xC2')
      {                         //in this I am interested
        i++;
        if (i >= sz)
        {                       //incomplete sequence(is this allowed to happen/how common?)
          p[i - 1] = ' ';
          DO_RET;
        }
        switch (p[i])
        {
        case '\x86':
        case '\x87':
          remove_char (p, i, sz);
          sz--;
          i--;
          p[i] = '*';
          break;
        case '\x8A':
          remove_char (p, i, sz);
          sz--;
          i--;
          p[i] = '\n';
          break;
        default:
          break;
        }
      }
      else
        i += 1;
    }                           //otherwise it's in ascii range..
  }
  DO_RET;
#undef DO_RET
}

/*
writes 1 to each position in q where a euro symbol should be placed in output..
0 otherwise
the euro sym in input is replaced with 'e'.
for ISO-6937 ..
*/
void
dvb_string_meme (char *p, char *q, int sz)
{
  while (sz--)
  {
    if (p[sz] == '\xA4')
    {
      p[sz] = 'e';
      q[sz] = 1;
    }
    else
      q[sz] = 0;
  }
}

/*
put an euro symbol to widechar string everywhere q[i]==1
*/
static void
dvb_string_rplew (wchar_t * p, char *q, int sz)
{
  while (sz--)
  {
    if (q[sz] == 1)
    {
//euro is e2 82 ac in utf8..
//0010 000010 101100
//should be 0x20ac 
      if (p[sz] != L'\0')
        p[sz] = L'\x20ac';      //will those end up in the right place?
    }
  }
}

#define TMPBUF_SZ 256
static char *
dvb_string_ascii (char *buf1, int len1, char *fromcode)
{
  iconv_t conv;
  char tmpbuf[TMPBUF_SZ + 1];
  char *rv = NULL;
  unsigned alloc_size = 0;
  size_t inbytes, outbytes;
  char *inbuf, *outbuf;
  int status = 0;
  char *buf;
  char *ebuf = NULL;
  int len = len1;
  buf = utlCalloc (len, 1);
  if (NULL == buf)
    return NULL;
  memmove (buf, buf1, len);
  //first: custom dvb sequences replacement


  //check for 1 byte inputs
  if (!strncmp (fromcode, "ISO8859", 7))
  {
    dvb_string_repl1b (buf, len);
  }
  else if (!strcmp (fromcode, "ISO6937"))
  {                             //also 8 bit but replace/remove euro code at 0xa4
    dvb_string_repl1b (buf, len);
    ebuf = utlCalloc (len, 1);
    if (NULL == ebuf)
    {
      utlFAN (buf);
      return NULL;
    }
    dvb_string_meme (buf, ebuf, len);
    utlFAN (ebuf);              //no Idea how to get those for ASCII..
  }
  else if (!strcmp (fromcode, "UTF8"))
  {                             //utf multibyte sequences
    dvb_string_replu8 (&buf, &len);
  }
  else if (!strcmp (fromcode, "ISO-10646/UCS2"))
  {                             //16 bit BMP.. not sure about those asian ones...
    dvb_string_repl2b ((uint16_t *) buf, len);
  }

  conv = iconv_open ("US-ASCII//TRANSLIT//IGNORE", fromcode);
  if ((iconv_t) - 1 == conv)
  {
    errMsg ("iconv_open failed: %s\n", strerror (errno));
    free (buf);
    return NULL;
  }
  inbuf = buf;
  inbytes = len;
  do
  {
    outbuf = tmpbuf;
    outbytes = TMPBUF_SZ;
    status = iconv (conv, &inbuf, &inbytes, &outbuf, &outbytes);
    if (status < 0)
    {

      if (errno == EINVAL)
      {

        /*
           now I'm getting an endless loop here with a single byte 0xE2
           (gdb) p inbuf
           $7 = 0x9fac467 <incomplete sequence \342>
           starts as:
           buf = 0x9fac270 "\025Panellist:\n\n• Bineta Diop, Founder and President, Femmes Africa Solidarit\n\n• Kristalina Georgieva, Chief Executive Officer, The World Bank\n\n• Thomas Greminger, Secretary-General, Organization f"...}
           (gdb) p (char *)(s.buf+140)
           $26 = 0x9fac2fc "ank\n\n• Thomas Greminger, Secretary-General, Organization for Security and Co-operation in Europe (OSCE)\n\n", <incomplete sequence \342>
           ...at the end..
         */
        status = 0;
        inbytes = 0;            //discard the rest..
      }
      else if (errno == E2BIG)
      {
        status = 0;             //benign.. just continue
      }
      else if (errno == EILSEQ)
      {                         //Invalid or incomplete multibyte or wide character(EILSEQ)
        // Zeitreise zurþck in die goldene Ära<- umlaut A (wrongly, by the broadcaster) fails here. would have to be combining but is single code... but 0xC4 is also a comb diaraesis(but with wrong 2nd char)
        //similar to EINVAL but there's data afterwards
        status = 0;
        inbytes = 0;
        /*can't just skip 1 byte.. might be more than 1 byte */
      }
    }
    if (status >= 0)
      utlSmartCatUt (&rv, &alloc_size, tmpbuf, TMPBUF_SZ - outbytes);
  }
  while ((inbytes > 0) && (status >= 0));
  iconv_close (conv);
  utlSmartCatDone (&rv);
  utlFAN (buf);
  return rv;
}

#define dvb_string_bmp_to_ascii(_BUF,_LEN) dvb_string_ascii(_BUF, _LEN, "ISO-10646/UCS2")
#define dvb_string_ksx1001_to_ascii(_BUF,_LEN) dvb_string_ascii(_BUF, _LEN, "EUC-KR")
#define dvb_string_GB2312_to_ascii(_BUF,_LEN) dvb_string_ascii(_BUF, _LEN, "GB2312")
#define dvb_string_Big5_to_ascii(_BUF,_LEN) dvb_string_ascii(_BUF, _LEN, "BIG5")
#define dvb_string_UTF8_to_ascii(_BUF,_LEN) dvb_string_ascii(_BUF, _LEN, "UTF8")
#define dvb_string_8859_1_to_ascii(_BUF,_LEN) dvb_string_ascii(_BUF, _LEN, "ISO8859-1")
#define dvb_string_8859_2_to_ascii(_BUF,_LEN) dvb_string_ascii(_BUF, _LEN, "ISO8859-2")
#define dvb_string_8859_3_to_ascii(_BUF,_LEN) dvb_string_ascii(_BUF, _LEN, "ISO8859-3")
#define dvb_string_8859_4_to_ascii(_BUF,_LEN) dvb_string_ascii(_BUF, _LEN, "ISO8859-4")
#define dvb_string_8859_5_to_ascii(_BUF,_LEN) dvb_string_ascii(_BUF, _LEN, "ISO8859-5")
#define dvb_string_8859_6_to_ascii(_BUF,_LEN) dvb_string_ascii(_BUF, _LEN, "ISO8859-6")
#define dvb_string_8859_7_to_ascii(_BUF,_LEN) dvb_string_ascii(_BUF, _LEN, "ISO8859-7")
#define dvb_string_8859_8_to_ascii(_BUF,_LEN) dvb_string_ascii(_BUF, _LEN, "ISO8859-8")
#define dvb_string_8859_9_to_ascii(_BUF,_LEN) dvb_string_ascii(_BUF, _LEN, "ISO8859-9")
#define dvb_string_8859_10_to_ascii(_BUF,_LEN) dvb_string_ascii(_BUF, _LEN, "ISO8859-10")
#define dvb_string_8859_11_to_ascii(_BUF,_LEN) dvb_string_ascii(_BUF, _LEN, "ISO8859-11")
#define dvb_string_8859_13_to_ascii(_BUF,_LEN) dvb_string_ascii(_BUF, _LEN, "ISO8859-13")
#define dvb_string_8859_14_to_ascii(_BUF,_LEN) dvb_string_ascii(_BUF, _LEN, "ISO8859-14")
#define dvb_string_8859_15_to_ascii(_BUF,_LEN) dvb_string_ascii(_BUF, _LEN, "ISO8859-15")

///return dyn allocated ascii string, NUL terminated. Non ascii chars (sometimes) replaced by ' '.
char *
dvbStringToAscii (DvbString * s)
{
  uint8_t a;
  if (s->len < 1)
    return strdup ("");
  a = *((uint8_t *) s->buf);
  if (a >= 0x20)                //(a<=0xff))
  {
    char *rv;
    rv = utlCalloc (s->len + 1, 1);
    if (NULL != rv)
    {
      memmove (rv, s->buf, s->len);
      rv[s->len] = '\0';
      dvb_string_ascii_sanitize (rv);
    }
    return rv;
  }
  else
  {
//    char *rv;
    switch (a)
    {

      // *@#%, whatever... I'm expecting this to break without setlocale...(like it's in dvbv-stream)
    case 1:
      return dvb_string_8859_5_to_ascii (s->buf + 1, s->len - 1);
    case 2:
      return dvb_string_8859_6_to_ascii (s->buf + 1, s->len - 1);
    case 3:
      return dvb_string_8859_7_to_ascii (s->buf + 1, s->len - 1);
    case 4:
      return dvb_string_8859_8_to_ascii (s->buf + 1, s->len - 1);
    case 5:
      return dvb_string_8859_9_to_ascii (s->buf + 1, s->len - 1);
    case 6:
      return dvb_string_8859_10_to_ascii (s->buf + 1, s->len - 1);
    case 7:
      return dvb_string_8859_11_to_ascii (s->buf + 1, s->len - 1);
      //                     case 8:
      //                     return dvb_string_8859_12_to_ucs4(s->buf+1,s->len-1);
    case 9:
      return dvb_string_8859_13_to_ascii (s->buf + 1, s->len - 1);
    case 0x0A:
      return dvb_string_8859_14_to_ascii (s->buf + 1, s->len - 1);
    case 0x0B:
      return dvb_string_8859_15_to_ascii (s->buf + 1, s->len - 1);

      /*
         perhaps fallback on this..
         case 1:
         case 2:
         case 3:
         case 4:
         case 5:
         case 6:
         case 7:
         case 8:
         case 9:
         case 0x0A:
         case 0x0B:
         rv = utlCalloc (s->len, 1);
         if (NULL != rv)
         {
         if (s->len > 1)
         memmove (rv, s->buf + 1, s->len - 1);

         rv[s->len - 1] = '\0';
         }
         dvb_string_ascii_sanitize (rv);
         return rv;
       */
    case 0x10:
      if (s->len < 3)
        return strdup ("");
      switch (s->buf[2])
      {
      case 1:
        return dvb_string_8859_1_to_ascii (s->buf + 3, s->len - 3);
      case 2:
        return dvb_string_8859_2_to_ascii (s->buf + 3, s->len - 3);
      case 3:
        return dvb_string_8859_3_to_ascii (s->buf + 3, s->len - 3);
      case 4:
        return dvb_string_8859_4_to_ascii (s->buf + 3, s->len - 3);
      case 5:
        return dvb_string_8859_5_to_ascii (s->buf + 3, s->len - 3);
      case 6:
        return dvb_string_8859_6_to_ascii (s->buf + 3, s->len - 3);
      case 7:
        return dvb_string_8859_7_to_ascii (s->buf + 3, s->len - 3);
      case 8:
        return dvb_string_8859_8_to_ascii (s->buf + 3, s->len - 3);
      case 9:
        return dvb_string_8859_9_to_ascii (s->buf + 3, s->len - 3);
      case 0x0A:
        return dvb_string_8859_10_to_ascii (s->buf + 3, s->len - 3);
      case 0x0B:
        return dvb_string_8859_11_to_ascii (s->buf + 3, s->len - 3);
      case 0x0D:
        return dvb_string_8859_13_to_ascii (s->buf + 3, s->len - 3);
      case 0x0E:
        return dvb_string_8859_14_to_ascii (s->buf + 3, s->len - 3);
      case 0x0F:
        return dvb_string_8859_15_to_ascii (s->buf + 3, s->len - 3);
      default:
        return strdup ("");
      }
/*
      rv = utlCalloc (s->len - 2, 1);
      if (NULL != rv)
      {
        if (s->len > 3)
          memmove (rv, s->buf + 3, s->len - 3);
        rv[s->len - 3] = '\0';
      }
      dvb_string_ascii_sanitize (rv);
      return rv;*/
    case 0x11:
      return dvb_string_bmp_to_ascii (s->buf + 1, s->len - 1);
    case 0x12:
      return dvb_string_ksx1001_to_ascii (s->buf + 1, s->len - 1);
    case 0x13:
      return dvb_string_GB2312_to_ascii (s->buf + 1, s->len - 1);
    case 0x14:
      return dvb_string_Big5_to_ascii (s->buf + 1, s->len - 1);
    case 0x15:
      return dvb_string_UTF8_to_ascii (s->buf + 1, s->len - 1);
    default:
      return strdup ("");
    }
  }
}

/*
ISO/IEC 10646 (2003): "Information technology - Universal Multiple-Octet Coded Character Set
(UCS)".

[44]        KSX1001: "Code for Information Interchange (Hangeul and Hanja)", Korean Agency for
            Technology and Standards, Ref. No. KSX 1001-2004.
NOTE: Available at http://unicode.org/Public//MAPPINGS/OBSOLETE/EASTASIA/KSC/KSX1001.TXT.

ETSI TR 101 162: "Digital Video Broadcasting (DVB); Allocation of Service Information (SI)
codes for DVB systems".
*/

static wchar_t *
dvb_string_w (char *buf1, int len1, char *fromcode)
{
  iconv_t conv;
  int status = 0;
  char tmpbuf[TMPBUF_SZ + sizeof (wchar_t)];
  char *rv = NULL;
  unsigned alloc_size = 0;
  size_t inbytes, outbytes;
  unsigned pos = 0;
  char *inbuf, *outbuf;
  char *buf;
  char *ebuf = NULL;
  int len = len1;
  buf = utlCalloc (len, 1);
  if (NULL == buf)
    return NULL;
  memmove (buf, buf1, len);
  //first: custom dvb sequences replacement


  //check for 1 byte inputs
  if (!strncmp (fromcode, "ISO8859", 7))
  {
    dvb_string_repl1b (buf, len);
  }
  else if (!strcmp (fromcode, "ISO6937"))
  {                             //also 8 bit but replace/remove euro code at 0xa4
    dvb_string_repl1b (buf, len);
    ebuf = utlCalloc (len, 1);
    if (NULL == ebuf)
    {
      utlFAN (buf);
      return NULL;
    }
    dvb_string_meme (buf, ebuf, len);
    utlFAN (ebuf);              //no Idea how to get those..
    //might not end up in the right places after conversion...
  }
  else if (!strcmp (fromcode, "UTF8"))
  {                             //utf multibyte sequences
    dvb_string_replu8 (&buf, &len);
  }
  else if (!strcmp (fromcode, "ISO-10646/UCS2"))
  {                             //16 bit BMP.. not sure about those asian ones...
    dvb_string_repl2b ((uint16_t *) buf, len);
  }

  //locale must be set up(correctly...utf8 etc..)  for those things to work... it's not entirely generic conversion...(esp transliteration..)

  conv = iconv_open ("WCHAR_T", fromcode);

  if ((iconv_t) - 1 == conv)
  {
    errMsg ("iconv_open failed: %s\n", strerror (errno));
    utlFAN (buf);
    utlFAN (ebuf);
    return NULL;
  }
  inbuf = buf;
  inbytes = len;
  do
  {
    outbuf = tmpbuf;
    outbytes = TMPBUF_SZ;
    status = iconv (conv, &inbuf, &inbytes, &outbuf, &outbytes);        //THROWS E2BIG instead of continuing piecemeal.. oh, it /does/ continue..
    if (status < 0)
    {

      if (errno == EINVAL)
      {

        /*
           now I'm getting an endless loop here with a single byte 0xE2
           (gdb) p inbuf
           $7 = 0x9fac467 <incomplete sequence \342>
           starts as:
           buf = 0x9fac270 "\025Panellist:\n\n• Bineta Diop, Founder and President, Femmes Africa Solidarit\n\n• Kristalina Georgieva, Chief Executive Officer, The World Bank\n\n• Thomas Greminger, Secretary-General, Organization f"...}
           (gdb) p (char *)(s.buf+140)
           $26 = 0x9fac2fc "ank\n\n• Thomas Greminger, Secretary-General, Organization for Security and Co-operation in Europe (OSCE)\n\n", <incomplete sequence \342>
           ...at the end..
         */
        status = 0;
        inbytes = 0;            //discard the rest..
      }
      else if (errno == E2BIG)
      {
        status = 0;             //benign.. just continue
      }
      else if (errno == EILSEQ)
      {                         //Invalid or incomplete multibyte or wide character(EILSEQ)
        // Zeitreise zurþck in die goldene Ära<- umlaut A (wrongly, by the broadcaster) fails here. would have to be combining but is single code... but 0xC4 is also a comb diaraesis(but with wrong 2nd char)
        //similar to EINVAL but there's data afterwards
        status = 0;
        inbytes = 0;
        /*can't just skip 1 byte.. might be more than 1 byte */
      }
    }
    if (status >= 0)
      utlSmartMemmove ((uint8_t **) & rv, &pos, &alloc_size,
                       (uint8_t *) tmpbuf, TMPBUF_SZ - outbytes);
  }
  while ((inbytes > 0) && (status >= 0));
  utlSmartMemmove ((uint8_t **) & rv, &pos, &alloc_size, (uint8_t *) L"", sizeof (wchar_t));    //the '\0' justincase..
  iconv_close (conv);
  utlSmartCatDoneW ((wchar_t **) & rv);
  utlFAN (buf);
  return (wchar_t *) rv;
}

#define dvb_string_ascii_to_w(_BUF,_LEN) dvb_string_w(_BUF, _LEN, "ASCII")
#define dvb_string_6937_to_w(_BUF,_LEN) dvb_string_w(_BUF, _LEN, "ISO6937")
#define dvb_string_8859_1_to_w(_BUF,_LEN) dvb_string_w(_BUF, _LEN, "ISO8859-1")
#define dvb_string_8859_2_to_w(_BUF,_LEN) dvb_string_w(_BUF, _LEN, "ISO8859-2")
#define dvb_string_8859_3_to_w(_BUF,_LEN) dvb_string_w(_BUF, _LEN, "ISO8859-3")
#define dvb_string_8859_4_to_w(_BUF,_LEN) dvb_string_w(_BUF, _LEN, "ISO8859-4")
#define dvb_string_8859_5_to_w(_BUF,_LEN) dvb_string_w(_BUF, _LEN, "ISO8859-5")
#define dvb_string_8859_6_to_w(_BUF,_LEN) dvb_string_w(_BUF, _LEN, "ISO8859-6")
#define dvb_string_8859_7_to_w(_BUF,_LEN) dvb_string_w(_BUF, _LEN, "ISO8859-7")
#define dvb_string_8859_8_to_w(_BUF,_LEN) dvb_string_w(_BUF, _LEN, "ISO8859-8")
#define dvb_string_8859_9_to_w(_BUF,_LEN) dvb_string_w(_BUF, _LEN, "ISO8859-9")
#define dvb_string_8859_10_to_w(_BUF,_LEN) dvb_string_w(_BUF, _LEN, "ISO8859-10")
#define dvb_string_8859_11_to_w(_BUF,_LEN) dvb_string_w(_BUF, _LEN, "ISO8859-11")
#define dvb_string_8859_13_to_w(_BUF,_LEN) dvb_string_w(_BUF, _LEN, "ISO8859-13")
#define dvb_string_8859_14_to_w(_BUF,_LEN) dvb_string_w(_BUF, _LEN, "ISO8859-14")
#define dvb_string_8859_15_to_w(_BUF,_LEN) dvb_string_w(_BUF, _LEN, "ISO8859-15")
#define dvb_string_bmp_to_w(_BUF,_LEN) dvb_string_w(_BUF, _LEN, "ISO-10646/UCS2")
#define dvb_string_ksx1001_to_w(_BUF,_LEN) dvb_string_w(_BUF, _LEN, "EUC-KR")
#define dvb_string_GB2312_to_w(_BUF,_LEN) dvb_string_w(_BUF, _LEN, "GB2312")
#define dvb_string_Big5_to_w(_BUF,_LEN) dvb_string_w(_BUF, _LEN, "BIG5")
#define dvb_string_UTF8_to_w(_BUF,_LEN) dvb_string_w(_BUF, _LEN, "UTF8")

///convert to wchar_t string
wchar_t *
dvbStringToW (DvbString * s)
{
  uint8_t a;
  if (s->len < 1)
    return (wchar_t *) wcsdup (L"");

  a = *((uint8_t *) s->buf);
  if (a >= 0x20)                //(a<=0xff))
  {
/*
this is ISO6937
but with euro symbol, non breaking space and soft hyphen..
*/
    return dvb_string_6937_to_w (s->buf, s->len);
  }
  else
  {
    switch (s->buf[0])
    {
    case 1:
      return dvb_string_8859_5_to_w (s->buf + 1, s->len - 1);
    case 2:
      return dvb_string_8859_6_to_w (s->buf + 1, s->len - 1);
    case 3:
      return dvb_string_8859_7_to_w (s->buf + 1, s->len - 1);
    case 4:
      return dvb_string_8859_8_to_w (s->buf + 1, s->len - 1);
    case 5:
      return dvb_string_8859_9_to_w (s->buf + 1, s->len - 1);
    case 6:
      return dvb_string_8859_10_to_w (s->buf + 1, s->len - 1);
    case 7:
      return dvb_string_8859_11_to_w (s->buf + 1, s->len - 1);
//                      case 8:
//                      return dvb_string_8859_12_to_w(s->buf+1,s->len-1);
    case 9:
      return dvb_string_8859_13_to_w (s->buf + 1, s->len - 1);
    case 0x0A:
      return dvb_string_8859_14_to_w (s->buf + 1, s->len - 1);
    case 0x0B:
      return dvb_string_8859_15_to_w (s->buf + 1, s->len - 1);
    case 0x10:
      if ((s->len < 3) || (s->buf[1] != 0x00))
        return (wchar_t *) wcsdup (L"");
      switch (s->buf[2])
      {
      case 1:
        return dvb_string_8859_1_to_w (s->buf + 3, s->len - 3);
      case 2:
        return dvb_string_8859_2_to_w (s->buf + 3, s->len - 3);
      case 3:
        return dvb_string_8859_3_to_w (s->buf + 3, s->len - 3);
      case 4:
        return dvb_string_8859_4_to_w (s->buf + 3, s->len - 3);
      case 5:
        return dvb_string_8859_5_to_w (s->buf + 3, s->len - 3);
      case 6:
        return dvb_string_8859_6_to_w (s->buf + 3, s->len - 3);
      case 7:
        return dvb_string_8859_7_to_w (s->buf + 3, s->len - 3);
      case 8:
        return dvb_string_8859_8_to_w (s->buf + 3, s->len - 3);
      case 9:
        return dvb_string_8859_9_to_w (s->buf + 3, s->len - 3);
      case 0x0A:
        return dvb_string_8859_10_to_w (s->buf + 3, s->len - 3);
      case 0x0B:
        return dvb_string_8859_11_to_w (s->buf + 3, s->len - 3);
      case 0x0D:
        return dvb_string_8859_13_to_w (s->buf + 3, s->len - 3);
      case 0x0E:
        return dvb_string_8859_14_to_w (s->buf + 3, s->len - 3);
      case 0x0F:
        return dvb_string_8859_15_to_w (s->buf + 3, s->len - 3);
      default:
        return (wchar_t *) wcsdup (L"");
      }
    case 0x11:
      return dvb_string_bmp_to_w (s->buf + 1, s->len - 1);
    case 0x12:
      return dvb_string_ksx1001_to_w (s->buf + 1, s->len - 1);
    case 0x13:
      return dvb_string_GB2312_to_w (s->buf + 1, s->len - 1);
    case 0x14:
      return dvb_string_Big5_to_w (s->buf + 1, s->len - 1);
    case 0x15:
      return dvb_string_UTF8_to_w (s->buf + 1, s->len - 1);
    default:
      return (wchar_t *) wcsdup (L"");
    }
  }
}

int
dvbStringWrite (FILE * f, DvbString * s)
{
  int tmp = 0;
  if ((s->len <= 0) || (NULL == s->buf))
    return (1 != fwrite (&tmp, sizeof (int), 1, f));

  return ((1 != fwrite (&s->len, sizeof (int), 1, f))
          || (1 != fwrite (s->buf, s->len, 1, f)));
}

int
dvbStringRead (FILE * f, DvbString * s)
{
  fread (&s->len, sizeof (int), 1, f);
  if (s->len <= 0)
  {
    s->len = 0;
    s->buf = NULL;
    return 0;
  }
  s->buf = utlCalloc (s->len, 1);
  if (NULL == s->buf)
  {
    s->len = 0;
    return 1;
  }
  return (1 != fread (s->buf, s->len, 1, f));
}

int
dvbStringRcv (int f, DvbString * s)
{
  ipcRcvS (f, s->len);
  if (s->len <= 0)
  {
    s->len = 0;
    s->buf = NULL;
    return 0;
  }

  s->buf = utlCalloc (s->len, 1);
  if (NULL == s->buf)
  {
    s->len = 0;
    return 1;
  }

  return (s->len != ioBlkRcv (f, s->buf, s->len));
}

int
dvbStringSnd (int f, DvbString * s)
{
  int tmp = 0;
  if ((s->len <= 0) || (NULL == s->buf))
    return ipcSndS (f, tmp);

  return (ipcSndS (f, s->len) || (s->len != ioBlkSnd (f, s->buf, s->len)));
}

void
dvbStringClear (DvbString * s)
{
  if (NULL != s->buf)
    utlFAN (s->buf);
  s->buf = NULL;
  s->len = 0;
}

int
dvbStringCopy (DvbString * to, DvbString * from)
{
  char *buf;
  if ((NULL == from->buf) || (0 >= from->len))
  {
    to->buf = NULL;
    to->len = 0;
    return 0;
  }
  buf = utlCalloc (from->len, 1);
  if (NULL != buf)
  {
    to->buf = buf;
    to->len = from->len;
    memmove (to->buf, from->buf, from->len);
    return 0;
  }
  return 1;
}

#ifdef TEST

int DF_BITS;
int DF_BTREE;
int DF_CUSTCK;
int DF_DHASH;
int DF_EPG_EVT;
int DF_FP_MM;
int DF_ITEM_LIST;
int DF_RCPTR;
int DF_SIZESTACK;
int DF_RS_ENTRY;
int DF_RCFILE;
int DF_TS;
int DF_SWITCH;
int DF_OPT;
int DF_CLIENT;
int DF_FAV;
int DF_FAV_LIST;

static char *
dvb_string_c (char *buf, int len, char *tocode, char *fromcode)
{
  iconv_t conv;
  char tmpbuf[TMPBUF_SZ + 1];
  int status;
  char *rv = NULL;
  unsigned alloc_size = 0, size = 0;
  size_t inbytes, outbytes;
  char *inbuf, *outbuf;
  conv = iconv_open (tocode, fromcode); //this does not actually seem to transliterate umlauts etc.. just generates '?'s or other garbage dependent on locale...(but garbage..)
  if ((iconv_t) - 1 == conv)
  {
    errMsg ("iconv_open failed: %s\n", strerror (errno));
    return NULL;
  }
  inbuf = buf;
  inbytes = len;
  do
  {
    outbuf = tmpbuf;
    outbytes = TMPBUF_SZ;
    status = iconv (conv, &inbuf, &inbytes, &outbuf, &outbytes);
    if (status >= 0)
      utlSmartMemmove ((uint8_t **) & rv, &size, &alloc_size, tmpbuf,
                       TMPBUF_SZ - outbytes);
  }
  while ((inbytes > 0) && (status >= 0));
  if (status < 0)
    printf ("error: %s\n", strerror (errno));
  utlSmartMemmove ((uint8_t **) & rv, &size, &alloc_size, (uint8_t *) L"", sizeof (wchar_t));   //the '\0' justincase..
  iconv_close (conv);
//      utlSmartCatDone(&rv);
  return rv;
}

unsigned
epg_get_txtlen (wchar_t * desc, unsigned width)
{
  unsigned l, x;
  wchar_t c;
  x = 0;
  l = 0;
  if (NULL == desc)
  {
    return 0;
  }
  while (L'\0' != (c = *desc))
  {
    int w = wcwidth (*desc);
    if (w <= 0)
      w = 1;                    //safer tat way(coz I think it gets width wrong..)
    x += w;
    if (x > width)
    {
      l++;
      x -= width;
    }
    if (c == L'\n')
    {
      l++;
      x = 0;
    }
    desc++;
  }
  return l + ((x + width - 1) / width);
}

//is the testcase OK?
//do I need escape sequences for the non-asii chars
//umlaut u =0xfc
//==test[5]
//yeah...
static char *test = "Es gr�nt so gr�n, wenn Spaniens Bl�ten bl�h'n\n���";
//static wchar_t *test_w = L"Es grünt so grün, wenn Spaniens Blüten blüh'n\näöü";

int
main (int argc, char *argv[])
{
  wchar_t *res;
  char *res2;
//  printf ("0x%hhx\n", test[5]);
  assert ('\xfc' == test[5] /*umlaut u==0xfc==test[5] */ );
  assert ('r' == test[4]);
  assert ('n' == test[6]);
  setlocale (LC_CTYPE, "de_AT.utf8");   //try them all.. It is FUN!!!! Oh! the utf8 does the trick...
  res = (wchar_t *) dvb_string_c (test, strlen (test) + 1, "WCHAR_T//TRANSLIT", "ISO8859-9");   //try them all IT IS FUN!!!
//  res2 = dvb_string_c ((char *) res, (wcslen (res) + 1) * 4, "ASCII", "UCS-4LE");   //try them all IT IS FUN!!!
//  fputws (res, stdout);//does not sho up...
//  fputws (L"endofline \n", stdout);//this is missing, too.. wrong stream orientation? Oh, yes.. without the printf(above), this shows up but stuff below does not..

//this works
//  res2 = dvb_string_c ((char *) test, (strlen (test) + 1), "UTF-8", "ISO8859-1");   //try them all IT IS FUN!!!
// question marks...
//  res2 = dvb_string_c ((char *) test, (strlen (test) + 1), "ASCII//TRANSLIT", "ISO8859-1");   //try them all IT IS FUN!!!
// nothing
//  res2 = dvb_string_c ((char *) test, (strlen (test) + 1), "ASCII", "ISO8859-1");   //try them all IT IS FUN!!!
//  res2 = dvb_string_c ((char *) test, (strlen (test) + 1), "ASCII//TRANSLIT", "ISO8859-1");   //try them all IT IS FUN!!!
//  res2 = dvb_string_c ((char *) test_w, (wcslen (test_w) + 1)*sizeof(wchar_t), "ASCII//TRANSLIT//IGNORE", "WCHAR_T");   //try them all IT IS FUN!!!
//  puts (res2);
  free (res);
//  free (res2);
  return 0;
}

/*
conv to ASCII fails for some reason...

supposedly ASCII is an alias for ANSI_X3.4-1968
but can not find a .so for that

I think it's in:
__gconv_transform_internal_ascii
WCHAR_T->ascii transform...
it bugs out with the codez above 0x7f

no.. should do translit in:
STANDARD_TO_LOOP_ERR_HANDLER

or it is dependent on order of transformations..

*/

#endif
