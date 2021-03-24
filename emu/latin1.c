#include "dat.h"

/*
 * The code makes two assumptions: strlen(ld) is 1 or 2; latintab[i].ld can be a
 * prefix of latintab[j].ld only when j<i.
 */
static
struct cvlist
{
	char	*ld;		/* must be seen before using this conversion */
	char	*si;		/* options for last input characters */
	char	*so;		/* the corresponding Rune for each si entry */
} latintab[] = {
	" ",  " i",		"␣ı",
	"!~", "-=~",		"≄≇≉",
	"!",  "!<=>?bmp",	"¡≮≠≯‽⊄∉⊅",
	"\"*","IUiu",		"ΪΫϊϋ",
	"\"", "AEIOUY\"aeiouy",	"ÄËÏÖÜŸ¨äëïöüÿ",
	"$*", "fhk",		"ϕϑϰ",
	"$",  "BEFHILMoRVaefglpv",	"ℬℰℱℋℐℒℳℴℛƲɑℯƒℊℓ℘ʋ",
	"'\"","Uu",		"Ǘǘ",
	"'",  "'ACEILNORSUYZacegilnorsuyz",
				"´ÁĆÉÍĹŃÓŔŚÚÝŹáćéģíĺńóŕśúýź",
	"*",  "*ABCDEFGHIKLMNOPQRSTUWXYZabcdefghiklmnopqrstuwxyz",
		"∗ΑΒΞΔΕΦΓΘΙΚΛΜΝΟΠΨΡΣΤΥΩΧΗΖαβξδεφγθικλμνοπψρστυωχηζ",
	"+",  "-O",		"±⊕",
	",",  ",ACEGIKLNORSTUacegiklnorstu",
				"¸ĄÇĘĢĮĶĻŅǪŖŞŢŲąçęģįķļņǫŗşţų",
	"-*", "l",		"ƛ",
	"-",  "+-2:>DGHILOTZbdghiltuz~",
				"∓­ƻ÷→ÐǤĦƗŁ⊖ŦƵƀðǥℏɨłŧʉƶ≂",
	".",  ".CEGILOZceglz",	"·ĊĖĠİĿ⊙Żċėġŀż",
	"/",  "Oo",		"Øø",
	"1",  "234568",		"½⅓¼⅕⅙⅛",
	"2",  "-35",		"ƻ⅔⅖",
	"3",  "458",		"¾⅗⅜",
	"4",  "5",		"⅘",
	"5",  "68",		"⅚⅝",
	"7",  "8",		"⅞",
	":",  "-=",		"÷≔",
	"<!", "=~",		"≨⋦",
	"<",  "-<=>~",		"←«≤≶≲",
	"=",  ":<=>OV",		"≕⋜≡⋝⊜⇒",
	">!", "=~",		"≩⋧",
	">",  "<=>~",		"≷≥»≳",
	"?",  "!?",		"‽¿",
	"@@",  "'EKSTYZekstyz",	"ьЕКСТЫЗекстыз",
	"@'",  "'",	"ъ",
	"@C",  "Hh",	"ЧЧ",
	"@E",  "Hh",	"ЭЭ",
	"@K",  "Hh",	"ХХ",
	"@S",  "CHch",	"ЩШЩШ",
	"@T",  "Ss",	"ЦЦ",
	"@Y",  "AEOUaeou",	"ЯЕЁЮЯЕЁЮ",
	"@Z",  "Hh",	"ЖЖ",
	"@c",  "h",	"ч",
	"@e",  "h",	"э",
	"@k",  "h",	"х",
	"@s",  "ch",	"щш",
	"@t",  "s",	"ц",
	"@y",  "aeou",	"яеёю",
	"@z",  "h",	"ж",
	"@",  "ABDFGIJLMNOPRUVXabdfgijlmnopruvx",
				"АБДФГИЙЛМНОПРУВХабдфгийлмнопрувх",
	"A",  "E",		"Æ",
	"C",  "ACU",		"⋂ℂ⋃",
	"Dv", "Zz",		"Ǆǅ",
	"D",  "-e",		"Ð∆",
	"G",  "-",		"Ǥ",
	"H",  "-H",		"Ħℍ",
	"I",  "-J",		"ƗĲ",
	"L",  "&-Jj|",		"⋀ŁǇǈ⋁",
	"N",  "JNj",		"Ǌℕǋ",
	"O",  "*+-./=EIcoprx",	"⊛⊕⊖⊙⊘⊜ŒƢ©⊚℗®⊗",
	"P",  "P",		"ℙ",
	"Q",  "Q",		"ℚ",
	"R",  "R",		"ℝ",
	"S",  "S",		"§",
	"T",  "-u",		"Ŧ⊨",
	"V",  "=",		"⇐",
	"Y",  "R",		"Ʀ",
	"Z",  "-Z",		"Ƶℤ",
	"^",  "ACEGHIJOSUWYaceghijosuwy",
				"ÂĈÊĜĤÎĴÔŜÛŴŶâĉêĝĥîĵôŝûŵŷ",
	"_\"","AUau",		"ǞǕǟǖ",
	"_.", "Aa",		"Ǡǡ",
	"_,", "Oo",		"Ǭǭ",
	"_",  "_AEIOUaeiou",	"¯ĀĒĪŌŪāēīōū",
	"`\"","Uu",		"Ǜǜ",
	"`",  "AEIOUaeiou",	"ÀÈÌÒÙàèìòù",
	"a",  "ben",		"↔æ∠",
	"b",  "()+-0123456789=bknpqru",
				"₍₎₊₋₀₁₂₃₄₅₆₇₈₉₌♝♚♞♟♛♜•",
	"c",  "$Oagu",		"¢©∩≅∪",
	"dv", "z",		"ǆ",
	"d",  "-adegz",		"ð↓‡°†ʣ",
	"e",  "ls",		"⋯∅",
	"f",  "a",		"∀",
	"g",  "$-r",		"¤ǥ∇",
	"h",  "-v",		"ℏƕ",
	"i",  "-bfjps",		"ɨ⊆∞ĳ⊇∫",
	"l",  "\"$&'-jz|",	"“£∧‘łǉ⋄∨",
	"m",  "iou",		"µ∈×",
	"n",  "jo",		"ǌ¬",
	"o",  "AOUaeiu",	"Å⊚Ůåœƣů",
	"p",  "Odgrt",		"℗∂¶∏∝",
	"r",  "\"'O",		"”’®",
	"s",  "()+-0123456789=abnoprstu",
				"⁽⁾⁺⁻⁰¹²³⁴⁵⁶⁷⁸⁹⁼ª⊂ⁿº⊃√ß∍∑",
	"t",  "-efmsu",		"ŧ∃∴™ς⊢",
	"u",  "-AEGIOUaegiou",	"ʉĂĔĞĬŎŬ↑ĕğĭŏŭ",
	"v\"","Uu",		"Ǚǚ",
	"v",  "ACDEGIKLNORSTUZacdegijklnorstuz",
				"ǍČĎĚǦǏǨĽŇǑŘŠŤǓŽǎčďěǧǐǰǩľňǒřšťǔž",
	"w",  "bknpqr",		"♗♔♘♙♕♖",
	"x",  "O",		"⊗",
	"y",  "$",		"¥",
	"z",  "-",		"ƶ",
	"|",  "Pp|",		"Þþ¦",
	"~!", "=",		"≆",
	"~",  "-=AINOUainou~",	"≃≅ÃĨÑÕŨãĩñõũ≈",
	0,	0,		0
};

/*
 * Given 5 characters k[0]..k[4], find the rune or return -1 for failure.
 */
static long
unicode(uchar *k)
{
	long i, c;

	k++;	/* skip 'X' */
	c = 0;
	for(i=0; i<4; i++,k++){
		c <<= 4;
		if('0'<=*k && *k<='9')
			c += *k-'0';
		else if('a'<=*k && *k<='f')
			c += 10 + *k-'a';
		else if('A'<=*k && *k<='F')
			c += 10 + *k-'A';
		else
			return -1;
	}
	return c;
}

/*
 * Given n characters k[0]..k[n-1], find the corresponding rune or return -1 for
 * failure, or something < -1 if n is too small.  In the latter case, the result
 * is minus the required n.
 */
long
latin1(uchar *k, int n)
{
	struct cvlist *l;
	int c;
	char* p;

	if(k[0] == 'X')
		if(n>=5)
			return unicode(k);
		else
			return -5;
	for(l=latintab; l->ld!=0; l++)
		if(k[0] == l->ld[0]){
			if(n == 1)
				return -2;
			if(l->ld[1] == 0)
				c = k[1];
			else if(l->ld[1] != k[1])
				continue;
			else if(n == 2)
				return -3;
			else
				c = k[2];
			for(p=l->si; *p!=0; p++)
				if(*p == c) {
					Rune r;
					int i = p - l->si;
					p = l->so;
					for(; i >= 0; i--)
						p += chartorune(&r, p);
					return r;
				}
			return -1;
		}
	return -1;
}
