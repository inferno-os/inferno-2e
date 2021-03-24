implement Img;

include "common.m";

# big tables in separate files
include "rgb.b";
include "ycbcr.b";

# local copies from CU
sys: Sys;
CU: CharonUtils;
	Header, ByteSource, MaskedImage, ImageCache, ResourceState: import CU;
D: Draw;
	Point, Rect, Image, Display: import D;
E: Events;
	Event: import E;
G: Gui;

dbg := 0;
dbgev := 0;
warn := 0;
progressive := 0;
display: ref D->Display;

init(cu: CharonUtils)
{
	sys = load Sys Sys->PATH;
	CU = cu;
	D = load Draw Draw->PATH;
	G = cu->G;
	init_tabs();
}

# Return true if mtype is an image type we can handle
supported(mtype: int) : int
{
	case mtype {
	CU->ImageJpeg or CU->ImageGif or CU->ImageXXBitmap or CU->ImageXInfernoBit =>
		return 1;
	}
	return 0;
}

# w,h passed in are specified width and height.
# Result will be resampled if they don't match the dimensions
# in the decoded picture (if only one of w,h is specified, the other
# dimension is scaled by the same factor).
ImageSource.new(bs: ref ByteSource, w, h: int) : ref ImageSource
{
	dbg = int (CU->config).dbg['i'];
	warn = (int (CU->config).dbg['w']) || dbg;
	dbgev = int (CU->config).dbg['e'];
	display = G->display;
	mtype := CU->UnknownType;
	if(bs.hdr != nil)
		mtype = bs.hdr.mtype;
	is := ref ImageSource(
		w,h,		# width, height
		0,0,		# origw, origh
		mtype,	# mtype
		0,		# i
		0,		# curframe
		bs,		# bs
		nil,		# ghdr
		nil,		# jhdr
		""		# err
		);
	return is;
}

ImageSource.free(is: self ref ImageSource)
{
	is.bs = nil;
	is.gstate = nil;
	is.jstate = nil;
}

ImageSource.getmim(is: self ref ImageSource) : (int, ref MaskedImage)
{
	if(dbg)
		sys->print("img: getmim\n");
	if(dbgev)
		CU->event("IMAGE_GETMIM", is.width*is.height);
	ex := ref sys->Exception;
	ans : ref MaskedImage = nil;
	ret := Mimnone;
	if(sys->rescue("exImageerror*", ex) == sys->EXCEPTION) {
		is.err = ex.name[14:];
		ret = Mimerror;
		if(dbg)
			sys->print("getmim got err: %s\n", is.err);
		sys->rescued(sys->ACTIVE, "");
	}
	else {
		if(is.bs.hdr == nil)
			return (Mimnone, nil);
		# temporary hack: wait until whole file is here first
		if(is.bs.edata == len is.bs.data) {
			if(is.mtype == CU->UnknownType) {
				u := is.bs.req.url;
				h := is.bs.hdr;
				h.setmediatype(u.path, is.bs.data);
				is.mtype = h.mtype;
			}
			case is.mtype {
			CU->ImageJpeg =>
				ans = getjpegmim(is);
			CU->ImageGif =>
				ans = getgifmim(is);
			CU->ImageXXBitmap =>
				ans = getxbitmapmim(is);
			CU->ImageXInfernoBit =>
				ans = getbitmim(is);
			* =>
				is.err = sys->sprint("unsupported image type %s", (CU->mnames)[is.mtype]);
				ret = Mimerror;
				ans = nil;
			}
			if(ans != nil)
				ret = Mimdone;
		}
		else {
			# slow down the spin-waiting for this image
			sys->sleep(100);
		}
	}
	if(dbg)
		sys->print("img: getmim returns (%d,%x)\n", ret, ans);
	if(dbgev)
		CU->event("IMAGE_GETMIM_END", 0);
	is.bs.lim = is.i;
	return (ret, ans);
}

# Raise exImagerror exception
imgerror(msg: string)
{
	if(dbg)
		sys->print("Image error: %s\n", msg);
	CU->raise("exImageerror: " + msg);
}

# Get next char or raise exception if cannot
getc(is: ref ImageSource) : int
{
	if(is.i >= len is.bs.data) {
		imgerror("premature eof");
	}
	return int is.bs.data[is.i++];
}

# Unget the last character.
# When called before any other getting routines, we
# know the buffer still has that character in it.
ungetc(is: ref ImageSource)
{
	if(is.i == 0)
		CU->raise("EXInternal: ungetc past beginning of buffer");
	is.i--;
}

# Like ungetc, but ungets two bytes (gotten in order b1, another char).
# Now the bytes could have spanned a boundary, if we were unlucky,
# so we have to be prepared to put b1 in front of current buffer.
ungetc2(is: ref ImageSource, nil: byte)
{
	if(is.i < 2) {
		if(is.i != 1)
			CU->raise("EXInternal: ungetc2 past beginning of buffer");
		is.i = 0;
	}
	else
		is.i -= 2;
}

# Get 2 bytes and return the 16-bit value, little-endian order.
getlew(is: ref ImageSource) : int
{
	c0 := getc(is);
	c1 := getc(is);
	return c0 + (c1<<8);
}

# Get 2 bytes and return the 16-bit value, big-endian order.
getbew(is: ref ImageSource) : int
{
	c0 := getc(is);
	c1 := getc(is);
	return (c0<<8) + c1;
}

# Copy next n bytes of input into buf
# or raise exception if cannot.
read(is: ref ImageSource, buf: array of byte, n: int)
{
	ok := 0;
	if(is.i +n < len is.bs.data) {
		buf[0:] = is.bs.data[is.i:is.i+n];
		is.i += n;
	}
	else
		imgerror("premature eof");
}

# Caller needs n bytes.
# Return an (array, index into array) where at least
# the next n bytes can be found.
# There might be a "premature eof" exception.
getn(is: ref ImageSource, n: int) : (array of byte, int)
{
	a := is.bs.data;
	i := is.i;
	if(i + n <= len a)
		is.i += n;
	else
		imgerror("premature eof");
	return (a, i);
}

# display.newimage with some defaults; throw exception if fails
newimage(w, h: int) : ref Image
{
	if(!(CU->imcache).need(w*h))
		imgerror("out of memory");
	im := display.newimage(((0,0),(w,h)), 3, 0, D->White);
	if(im == nil)
		imgerror("out of memory");
	return im;
}

newmi(im: ref Image) : ref MaskedImage
{
	return ref MaskedImage(im, nil, 0, 0, -1, Point(0,0));
}

# Call this after origw and origh are set to set the width and height
# to our desired (rescaled) answer dimensions.
# If only one of the dimensions is specified, the other is scaled by
# the same factor.
setdims(is: ref ImageSource)
{
	sw := is.origw;
	sh := is.origh;
	dw := is.width;
	dh := is.height;
	if(dw == 0 && dh == 0) {
		dw = sw;
		dh = sh;
	}
	else if(dw == 0 || dh == 0) {
		if(dw == 0) {
			dw = int ((real sw) * (real dh/real sh));
			if(dw == 0)
				dw = 1;
		}
		else {
			dh = int ((real sh) * (real dw/real sw));
			if(dh == 0)
				dh = 1;
		}
	}
	is.width = dw;
	is.height = dh;
}

# for debugging
printarray(a: array of int, name: string)
{
	sys->print("%s:", name);
	for(i := 0; i < len a; i++) {
		if((i%10)==0)
			sys->print("\n%5d: ", i);
		sys->print("%6d", a[i]);
	}
	sys->print("\n");
}

################# XBitmap ###################

getxbitmaphdr(is: ref ImageSource)
{
	fnd: int;
	(fnd, is.origw) = getxbitmapdefine(is);
	if(fnd)
		(fnd, is.origh) = getxbitmapdefine(is);
	if(!fnd)
		imgerror("xbitmap starts badly");
	# now, optional x_hot, y_hot
	(fnd, nil) = getxbitmapdefine(is);
	if(fnd)
		(fnd, nil) = getxbitmapdefine(is);
	# now expect 'static char x...x_bits[] = {'
	get_to_char(is, '{');
}

getxbitmapmim(is: ref ImageSource) : ref MaskedImage
{
	getxbitmaphdr(is);
	setdims(is);
	bytesperline := (is.origw+7) / 8;
	pixels := array[is.origw*is.origh] of byte;
	pixi := 0;
	for(i := 0; i < is.origh; i++) {
		for(j := 0; j < bytesperline; j++) {
			v := get_hexbyte(is);
			kend := 7;
			if(j == bytesperline-1)
				kend = (is.origw-1)%8;
			for(k := 0; k <= kend; k++) {
				if(v & (1<<k))
					pixels[pixi] = byte D->Black;
				else
					pixels[pixi] = byte D->White;
				pixi++;
			}
		}
	}
	if(is.width != is.origw || is.height != is.origh)
		pixels = resample(pixels, is.origw, is.origh, is.width, is.height);
	im := newimage(is.width, is.height);
	im.writepixels(im.r, pixels);
	return newmi(im);
}

# get a line, which should be of form
#	'#define fieldname val'
# and return (found, integer rep of val)
getxbitmapdefine(is: ref ImageSource) : (int, int)
{
	fnd := 0;
	n := 0;
	c := getc(is);
	if(c == '#') {
		get_to_char(is, ' ');
		get_to_char(is, ' ');
		c = getc(is);
		while(c >= '0' && c <= '9') {
			fnd = 1;
			n = n*10 + c - '0';
			c = getc(is);
		}
	}
	else
		ungetc(is);
	get_to_char(is, '\n');
	return (fnd, n);
}

# read fd until get char cterm
# (raise exception if eof hit first)
get_to_char(is: ref ImageSource, cterm: int)
{
	for(;;) {
		if(getc(is) == cterm)
			return;
	}
}

# read fd until get xDD, were DD are hex digits.
# (raise exception if not hex digits or if eof hit first)
get_hexbyte(is: ref ImageSource) : int
{
	get_to_char(is, 'x');
	n1 := hexdig(getc(is));
	n2 := hexdig(getc(is));
	if(n1 < 0 || n2 < 0)
		imgerror("X Bitmap expected hex digits");
	return (n1<<4) + n2;
}

hexdig(c: int) : int
{
	if('0' <= c && c <= '9')
		c -= '0';
	else if('a' <= c && c <= 'f')
		c += 10 - 'a';
	else if('A' <= c && c <= 'F')
		c += 10 - 'A';
	else
		c = -1;
	return c;
}

################# GIF ###################

# GIF flags
TRANSP:		con 1;
INPUT:		con 2;
DISPMASK:	con 7<<2;
HASCMAP:	con 16r80;
INTERLACED:	con 16r40;

Entry: adt
{
	prefix: int;
	exten: int;
};

getgifhdr(is: ref ImageSource)
{
	if(dbg)
		sys->print("img: getgifhdr\n");
	h := ref Gifstate;
	(buf, i) := getn(is, 6);
	vers := string buf[i:i+6];
	if(vers!="GIF87a" && vers!="GIF89a")
		imgerror("unknown GIF version " + vers);
	is.origw = getlew(is);
	is.origh = getlew(is);
	h.fields = getc(is);
	h.bgrnd = getc(is);
	h.aspect = getc(is);
	setdims(is);
	if(dbg)
		sys->print("img: getgifhdr has vers=%s, origw=%d, origh=%d, w=%d, h=%d, fields=16r%x, bgrnd=%d, aspect=%d\n",
			vers, is.origw, is.origh, is.width, is.height, h.fields, h.bgrnd, h.aspect);
	h.flags = 0;
	h.delay = 0;
	h.trindex = byte 0;
	h.tbl = array[4096] of GifEntry;
	for(i = 0; i < 258; i++) {
		h.tbl[i].prefix = -1;
		h.tbl[i].exten = i;
	}
	h.globalcmap = nil;
	h.cmap = nil;
	if(h.fields & HASCMAP)
		h.globalcmap = gifreadcmap(is, (h.fields&7)+1);
	is.gstate = h;
	if(warn && h.aspect != 0)
		sys->print("warning: non-standard aspect ratio in GIF image ignored\n");
	if(!gifgettoimage(is))
		imgerror("GIF file has no image");
}

gifgettoimage(is: ref ImageSource) : int
{
	if(dbg)
		sys->print("img: gifgettoimage\n");
	h := is.gstate;
loop:
	for(;;) {
		# some GIFs omit Trailer
		if(is.i >= len is.bs.data)
			break;
		case c := getc(is) {
		16r2C =>	# Image Descriptor
			return 1;

		16r21 =>	# Extension
			hsize := 0;
			hasdata := 0;
		
			case getc(is){
			16r01 =>	# Plain Text Extension
				hsize = 14;
				hasdata = 1;
				if(dbg)
					sys->print("gifgettoimage: text extension\n");
			16rF9 =>	# Graphic Control Extension
				getc(is);	# blocksize (should be 4)
				h.flags = getc(is);
				h.delay = getlew(is);
				h.trindex = byte getc(is);
				getc(is);	# block terminator (should be 0)
				if(dbg)
					sys->print("gifgettoimage: graphic control flags=16r%x, delay=%d, trindex=%d\n",
						h.flags, h.delay, int h.trindex);
			16rFE =>	# Comment Extension
				if(dbg)
					sys->print("gifgettoimage: comment extension\n");
				hasdata = 1;
			16rFF =>	# Application Extension
				if(dbg)
					sys->print("gifgettoimage: application extension\n");
				hsize = getc(is);
				# standard says this must be 11, but Adobe likes to put out 10-byte ones,
				# so we pay attention to the field.
				hasdata = 1;
			* =>
				imgerror("GIF unknown extension");
			}
			if(hsize > 0)
				getn(is, hsize);
			if(hasdata) {
				for(;;) {
					if((nbytes := getc(is)) == 0)
						break;
					(a, i) := getn(is, nbytes);
					if(dbg)
						sys->print("extension data: '%s'\n", string a[i:i+nbytes]);
				}
			}

		16r3B =>	# Trailer
			# read to end of data
			getn(is, len is.bs.data - is.i);
			break loop;

		* =>
			if(c == 0)
				continue;		# FIX for some buggy gifs
			imgerror("GIF unknown block type " + string c);
		}
	}
	return 0;
}

getgifmim(is: ref ImageSource) : ref MaskedImage
{
	if(is.gstate == nil)
		getgifhdr(is);

	# At this point, should just have read Image Descriptor marker byte
	h := is.gstate;
	left :=getlew(is);
	top := getlew(is);
	width := getlew(is);
	height := getlew(is);
	h.fields = getc(is);
	totpix := width*height;
	h.cmap = h.globalcmap;
	if(dbg)
		sys->print("getgifmim, left=%d, top=%d, width=%d, height=%d, fields=16r%x\n",
			left, top, width, height, h.fields);
	if(dbgev)
		CU->event("IMAGE_GETGIFMIM", 0);
	if(h.fields & HASCMAP)
		h.cmap = gifreadcmap(is, (h.fields&7)+1);
	if(h.cmap == nil)
		imgerror("GIF needs colormap");

	# now decode the image
	c, incode: int;

	codesize := getc(is);
	if(codesize > 8)
		imgerror("GIF bad codesize");
	if(len h.cmap!=3*(1<<codesize) 
	  && len h.cmap != 3*(1<<(codesize-1))	# peculiar GIF bitmap files
	  && (codesize!=2 || len h.cmap!=3*2)){ # peculiar GIF bitmap files II
		if (warn)
			sys->print("warning: GIF codesize = %d doesn't match cmap len = %d\n", codesize, len h.cmap);
		#imgerror("GIF codesize doesn't match color map");
	}

	CTM :=1<<codesize;
	EOD := CTM+1;

	pic := array[totpix] of byte;
	pici := 0;
	data : array of byte = nil;
	datai := 0;
	dataend := 0;

	nbits := 0;
	sreg := 0;
	stack := array[4096] of byte;
	stacki: int;
	fc := 0;
	tbl := h.tbl;

Decode:
	for(;;) {
		csize := codesize+1;
		csmask := ((1<<csize) - 1);
		nentry := EOD+1;
		maxentry := csmask;
		first := 1;
		ocode := -1;

		for(;; ocode = incode) {
			while(nbits < csize) {
				if(datai == dataend) {
					nbytes := getc(is);
					if(nbytes == 0)
						break Decode;
					(data, datai) = getn(is, nbytes);
					dataend = datai+nbytes;
				}
				c = int data[datai++];
				sreg |= c<<nbits;
				nbits += 8;
			}
			code := sreg & csmask;
			sreg >>= csize;
			nbits -= csize;

			if(code == EOD) {
				nbytes := getc(is);
				if(nbytes != 0 && warn)
					sys->print("warning: unexpected data past EOD\n");
				break Decode;
			}

			if(code == CTM)
				continue Decode;

			stacki = len stack-1;

			incode = code;

			# special case for KwKwK 
			if(code == nentry) {
				stack[stacki--] = byte fc;
				code = ocode;
			}

			if(code > nentry)
				imgerror("GIF bad code");
		
			for(c=code; c>=0; c=tbl[c].prefix)
				stack[stacki--] = byte tbl[c].exten;

			nb := len stack-(stacki+1);
			if(pici+nb > len pic) {
				# this common error is harmless
				# we have to keep reading to keep the blocks in sync
				;
			}
			else {
				pic[pici:] = stack[stacki+1:];
				pici += nb;
			}

			fc = int stack[stacki+1];

			if(first) {
				first = 0;
				continue;
			}
			early:=0; # peculiar tiff feature here for reference
			if(nentry == maxentry-early) {
				if(csize >= 12)
					continue;
				csize++;
				maxentry = (1<<csize);
				csmask = maxentry - 1;
				if(csize < 12)
					maxentry--;
			}
			tbl[nentry].prefix = ocode;
			tbl[nentry].exten = fc;
			nentry++;
		}
	}
	while(pici < len pic) {
		# shouldn't happen, but sometimes get buggy gifs
		pic[pici++] = byte 0;
	}

	if(h.fields & INTERLACED) {
		if(dbg)
			sys->print("getgifmim uninterlacing\n");
		if(dbgev)
			CU->event("IMAGE_GETGIFMIM_INTERLACE_START", 0);
		# (TODO: Could un-interlace in place.
		# Decompose permutation into cycles,
		# then need one double-copy of a line
		# per cycle).
		ipic := array[totpix] of byte;
		# Group 1: every 8th row, starting with row 0
		pici = 0;
		ipici := 0;
		ipiclim := totpix-width;
		w2 := width+width;
		w4 := w2+w2;
		w8 := w4+w4;
		startandby := array[4] of {(0,w8), (w4,w8), (w2,w4), (width,w2)};
		for(k := 0; k < 4; k++) {
			(start, by) := startandby[k];
			for(ipici=start; ipici <= ipiclim; ipici += by) {
				ipic[ipici:] = pic[pici:pici+width];
				pici += width;
			}
		}
		pic = ipic;
		if(dbgev)
			CU->event("IMAGE_GETGIFMIM_INTERLACE_END", 0);
	}
	if(is.width != is.origw || is.height != is.origh) {
		# need to resample, using same factors as original image
		wscale := real is.width / real is.origw;
		hscale := real is.height / real is.origh;
		owidth := width;
		oheight := height;
		width = int (wscale * real width);
		if(width == 0)
			width = 1;
		height = int (hscale * real height);
		if(height == 0)
			height = 1;
		left = int (wscale * real left);
		top = int (hscale * real top);
		pic = resample(pic, owidth, oheight, width, height);
	}
	mask : ref Image;
	if(h.flags & TRANSP) {
		if(dbg)
			sys->print("getgifmim making mask, trindex=%d\n", int h.trindex);
		if(dbgev)
			CU->event("IMAGE_GETGIFMIM_MASK_START", 0);
		# make a 1-bit deep bitmap for mask
		# expect most mask bits will be 1
		bytesperrow := (width+7)/8;
		trpix := h.trindex;
		mpic := array[bytesperrow*height] of byte;
		mpici := 0;
		pici = 0;
		for(y := 0; y < height; y++) {
			v := byte 16rFF;
			k := 0;
			for(x := 0; x < width; x++) {
				if(pic[pici++] == trpix)
					v &= ~(byte 16r80>>k);
				if(++k == 8) {
					k = 0;
					mpic[mpici++] = v;
					v = byte 16rFF;
				}
			}
			if(k != 0)
				mpic[mpici++] = v;
		}
		if(!(CU->imcache).need(bytesperrow*height))
			imgerror("out of memory");
		mask = display.newimage(((0,0),(width,height)), 0, 0, 1);
		if(mask == nil)
			imgerror("out of memory");
		mask.writepixels(mask.r, mpic);
		mpic = nil;
		if(dbgev)
			CU->event("IMAGE_GETGIFMIM_MASK_END", 0);
	}
	if(dbgev)
		CU->event("IMAGE_GETGIFMIM_REMAP_START", 0);
	remap1(pic, width, height, h.cmap);
	if(dbgev)
		CU->event("IMAGE_GETGIFMIM_REMAP_END", 0);
	bgcolor := -1;
	i := h.bgrnd;
	if(i >= 0 && 3*i+2 < len h.cmap) {
		bgcolor = ((int h.cmap[3*i])<<16)
			| ((int h.cmap[3*i+1])<<8)
			| (int h.cmap[3*i+2]);
	}
	im := newimage(width, height);
	im.writepixels(im.r, pic);
	if(is.curframe == 0) {
		# make sure first frame fills up whole rectangle
		if(is.width != width || is.height != height || left != 0 || top != 0) {
			r := Rect((left,top),(left+width,top+height));
			pix := D->White;
			if(bgcolor != -1)
				pix = closest_rgbpix((bgcolor>>16)&255, (bgcolor>>8)&255, bgcolor&255);
			newim := display.newimage(((0,0),(is.width,is.height)), 3, 0, pix);
			if(newim == nil)
				imgerror("out of memory");
			newim.draw(r, im, mask, (0,0));
			im = newim;
			if(mask != nil) {
				newmask := display.newimage(((0,0),(is.width,is.height)), 0, 0, 0);
				if(newmask == nil)
					imgerror("out of memory");
				newmask.draw(r, mask, nil, (0,0));
				mask = newmask;
			}
			left = 0;
			top = 0;
		}
	}
	pic = nil;
	mi := newmi(im);
	mi.mask = mask;
	mi.delay = h.delay*10;	# convert centiseconds to milliseconds
	mi.origin = Point(left, top);
	dispmeth := (h.flags>>2)&7;
	if(dispmeth == 2) {
		# reset to background color after displaying this frame
		mi.bgcolor = bgcolor;
	}
	else if(dispmeth == 3) {
		# Supposed to "reset to previous", which appears to
		# mean the previous frame that didn't have a "reset to previous".
		# Signal this special case to layout by setting bgcolor to -2.
		mi.bgcolor = -2;
	}
	if(gifgettoimage(is)) {
		mi.more = 1;
		is.curframe++;
		# have to reinitialize table for next time
		for(i = 0; i < 258; i++) {
			h.tbl[i].prefix = -1;
			h.tbl[i].exten = i;
		}
	}
	if(dbgev)
		CU->event("IMAGE_GETGIFMIM_END", 0);
	return mi;	
}

# Read a GIF colormap, where bpe is number of bits in an entry.
# Raises a 'premature eof' exception if can't get the whole map.
gifreadcmap(is: ref ImageSource, bpe: int) : array of byte
{
	size := 3*(1<<bpe);
	map := array[size] of byte;
	if(dbg > 1)
		sys->print("gifreadcmap wants %d bytes\n", size);
	read(is, map, size);
	return map;
}

################# JPG ###################

# Constants, all preceded by byte 16rFF
SOF:	con 16rC0;	# Start of Frame
SOF2:	con 16rC2;	# Start of Frame; progressive Huffman
JPG:	con 16rC8;	# Reserved for JPEG extensions
DHT:	con 16rC4;	# Define Huffman Tables
DAC:	con 16rCC;	# Arithmetic coding conditioning
RST:	con 16rD0;	# Restart interval termination
RST7:	con 16rD7;	# Restart interval termination (highest value)
SOI:	con 16rD8;	# Start of Image
EOI:	con 16rD9;	# End of Image
SOS:	con 16rDA;	# Start of Scan
DQT:	con 16rDB;	# Define quantization tables
DNL:	con 16rDC;	# Define number of lines
DRI:	con 16rDD;	# Define restart interval
DHP:	con 16rDE;	# Define hierarchical progression
EXP:	con 16rDF;	# Expand reference components
APPn:	con 16rE0;	# Reserved for application segments
JPGn:	con 16rF0;	# Reserved for JPEG extensions
COM:	con 16rFE;	# Comment

NBUF:	con 16*1024;

# channel descriptions
CRGB:   con 0;  # three channels, R, G, B, no map
CY:     con 1;  # one channel, luminance
CRGB1:  con 2;  # one channel, map present
CYCbCr:  con 3;  # three channels, Y, Cb, Cr, no map


jpegcolorspace: con CYCbCr;

zerobytes := array[64] of { * => byte 0 };
zeroints := array[64] of { * => 0 };

getjpeghdr(is: ref ImageSource)
{
	if(dbg)
		sys->print("getjpeghdr\n");
	h := ref Jpegstate(
		0, 0,		# sr, cnt
		0,		# Nf
		nil,		# comp
		byte 0,	# mode,
		0, 0,		# X, Y
		nil,		# qt
		nil, nil,	# dcht, acht
		0,		# Ns
		nil,		# scomp
		0, 0,		# Ss, Se
		0, 0,		# Ah, Al
		0, 0,		# ri, nseg
		nil,		# nblock
		nil, nil,	# dccoeff, accoeff
		0, 0, 0, 0	# nacross, ndown, Hmax, Vmax
		);
	is.jstate = h;
	if(jpegmarker(is) != SOI)
		imgerror("Jpeg expected SOI marker");
	(m, n) := jpegtabmisc(is);
	if(!(m == SOF || m == SOF2))
		imgerror("Jpeg expected Frame marker");
	nil = getc(is);		# sample precision
	h.Y = getbew(is);
	h.X = getbew(is);
	h.Nf = getc(is);
	if(dbg)
		sys->print("start of frame, Y=%d, X=%d, Nf=%d\n", h.Y, h.X, h.Nf);
	h.comp = array[h.Nf] of Framecomp;
	h.nblock = array[h.Nf] of int;
	for(i:=0; i<h.Nf; i++) {
		h.comp[i].C = getc(is);
		(H, V) := nibbles(getc(is));
		h.comp[i].H = H;
		h.comp[i].V = V;
		h.comp[i].Tq = getc(is);
		h.nblock[i] =H*V;
		if(dbg)
			sys->print("comp[%d]: C=%d, H=%d, V=%d, Tq=%d\n",
				i, h.comp[i].C, H, V, h.comp[i].Tq);
	}
	h.mode = byte m;
	is.origw = h.X;
	is.origh = h.Y;
	setdims(is);
	if(n != 6+3*h.Nf)
		imgerror("Jpeg bad SOF length");
}

jpegmarker(is: ref ImageSource) : int
{
	if(getc(is) != 16rFF)
		imgerror("Jpeg expected marker");
	return getc(is);
}

# Consume tables and miscellaneous marker segments,
# returning the marker id and length of the first non-such-segment
# (after having consumed the marker).
# May raise "premature eof" or other exception.
jpegtabmisc(is: ref ImageSource) : (int, int)
{
	h := is.jstate;
	m, n : int;
Loop:
	for(;;) {
		h.nseg++;
		m = jpegmarker(is);
		n = 0;
		if(m != EOI)
			n = getbew(is) - 2;
		if(dbg > 1)
			sys->print("jpegtabmisc reading segment, got m=%x, n=%d\n", m, n);
		case m {
		SOF or SOF2 or SOS or EOI =>
			break Loop;

		APPn+0 =>
			if(h.nseg==1 && n >= 6) {
				(buf, i) := getn(is, 6);
				n -= 6;
				if(string buf[i:i+4]=="JFIF") {
					vers0 := int buf[i+5];
					vers1 := int buf[i+6];
					if(vers0>1 || vers1>2)
						imgerror("Jpeg unimplemented version");
				}
			}

		APPn+1 to APPn+15 =>
			;

		DQT =>
			jpegquanttables(is, n);
			n = 0;

		DHT =>
			jpeghuffmantables(is, n);
			n = 0;

		DRI =>
			h.ri =getbew(is);
			n -= 2;

		COM =>
			;

		* =>
			imgerror("Jpeg unexpected marker");
		}
		if(n > 0)
			getn(is, n);
	}
	return (m, n);
}

# Consume huffman tables, raising exception on error.
jpeghuffmantables(is: ref ImageSource, n: int)
{
	if(dbg)
		sys->print("jpeghuffmantables\n");
	h := is.jstate;
	if(h.dcht == nil) {
		h.dcht = array[4] of ref Huffman;
		h.acht = array[4] of ref Huffman;
	}
	for(l:= 0; l < n; )
		l += jpeghuffmantable(is);
	if(l != n)
		imgerror("Jpeg huffman table bad length");
}

jpeghuffmantable(is: ref ImageSource) : int
{
	t := ref Huffman;
	h := is.jstate;
	(Tc, th) := nibbles(getc(is));
	if(dbg > 1)
		sys->print("jpeghuffmantable, Tc=%d, th=%d\n", Tc, th);
	if(Tc > 1)
		imgerror("Jpeg unknown Huffman table class");
	if(th>3 || (h.mode==byte SOF && th>1))
		imgerror("Jpeg unknown Huffman table index");
	if(Tc == 0)
		h.dcht[th] = t;
	else
		h.acht[th] = t;

	# flow chart C-2
	(b, bi) := getn(is, 16);
	numcodes := array[16] of int;
	nsize := 0;
	for(i:=0; i<16; i++)
		nsize += (numcodes[i] = int b[bi+i]);
	t.size = array[nsize+1] of int;
	k := 0;
	for(i=1; i<=16; i++) {
		n :=numcodes[i-1];
		for(j:=0; j<n; j++)
			t.size[k++] = i;
	}
	t.size[k] = 0;

	# initialize HUFFVAL
	t.val = array[nsize] of int;
	(b, bi) = getn(is, nsize);
	for(i=0; i<nsize; i++)
		t.val[i] = int b[bi++];

	# flow chart C-3
	t.code = array[nsize+1] of int;
	k = 0;
	code := 0;
	si := t.size[0];
	for(;;) {
		do
			t.code[k++] = code++;
		while(t.size[k] == si);
		if(t.size[k] == 0)
			break;
		do {
			code <<= 1;
			si++;
		} while(t.size[k] != si);
	}

	# flow chart F-25
	t.mincode = array[17] of int;
	t.maxcode = array[17] of int;
	t.valptr = array[17] of int;
	i = 0;
	j := 0;
    F25:
	for(;;) {
		for(;;) {
			i++;
			if(i > 16)
				break F25;
			if(numcodes[i-1] != 0)
				break;
			t.maxcode[i] = -1;
		}
		t.valptr[i] = j;
		t.mincode[i] = t.code[j];
		j += int numcodes[i-1]-1;
		t.maxcode[i] = t.code[j];
		j++;
	}

	# create byte-indexed fast path tables
	t.value = array[256] of int;
	t.shift = array[256] of int;
	maxcode := t.maxcode;
	# stupid startup algorithm: just run machine for each byte value
  Bytes:
	for(v:=0; v<256; v++){
		cnt := 7;
		m := 1<<7;
		code = 0;
		sr := v;
		i = 1;
		for(;;i++){
			if(sr & m)
				code |= 1;
			if(code <= maxcode[i])
				break;
			code <<= 1;
			m >>= 1;
			if(m == 0){
				t.shift[v] = 0;
				t.value[v] = -1;
				continue Bytes;
			}
			cnt--;
		}
		t.shift[v] = 8-cnt;
		t.value[v] = t.val[t.valptr[i]+(code-t.mincode[i])];
	}
	if(dbg > 2) {
		sys->print("Huffman table %d:\n", th);
		printarray(t.size, "size");
		printarray(t.code, "code");
		printarray(t.val, "val");
		printarray(t.mincode, "mincode");
		printarray(t.maxcode, "maxcode");
		printarray(t.value, "value");
		printarray(t.shift, "shift");
	}

	return nsize+17;
}

jpegquanttables(is: ref ImageSource, n: int)
{
	if(dbg)
		sys->print("jpegquanttables\n");
	h := is.jstate;
	if(h.qt == nil)
		h.qt = array[4] of array of int;
	for(l:=0; l<n; )
		l += jpegquanttable(is);
	if(l != n)
		imgerror("Jpeg quant table bad length");
}

jpegquanttable(is: ref ImageSource): int
{
	(pq, tq) := nibbles(getc(is));
	if(dbg)
		sys->print("jpegquanttable pq=%d tq=%d\n", pq, tq);
	if(pq > 1)
		imgerror("Jpeg unknown quantization table class");
	if(tq > 3)
		imgerror("Jpeg bad quantization table index");
	q := array[64] of int;
	is.jstate.qt[tq] = q;
	for(i:=0; i<64; i++) {
		if(pq == 0)
			q[i] =getc(is);
		else
			q[i] = getbew(is);
	}
	if(dbg > 2)
		printarray(q, "quant table");
	return 1+(64*(1+pq));;
}

# Have just read Frame header.
# Now expect:
#	((tabl/misc segment(s))* (scan header) (entropy coded segment)+)+ EOI
getjpegmim(is: ref ImageSource) : ref MaskedImage
{
	if(dbg)
		sys->print("getjpegmim\n");
	if(dbgev)
		CU->event("IMAGE_GETJPGMIM", is.width*is.height);
	getjpeghdr(is);
	h := is.jstate;
	chans: array of array of byte = nil;
	for(;;) {
		(m, n) := jpegtabmisc(is);
		if(m == EOI)
			break;
		if(m != SOS)
			imgerror("Jpeg expected start of scan");
		h.Ns = getc(is);
		if(dbg)
			sys->print("start of scan, Ns=%d\n", h.Ns);
		scomp := array[h.Ns] of Scancomp;
		for(i := 0; i < h.Ns; i++) {
			scomp[i].C = getc(is);
			(scomp[i].tdc, scomp[i].tac) = nibbles(getc(is));
		}
		h.scomp = scomp;
		h.Ss = getc(is);
		h.Se = getc(is);
		(h.Ah, h.Al) = nibbles(getc(is));
		if(n != 4+h.Ns*2)
			imgerror("Jpeg SOS header wrong length");

		if(h.mode == byte SOF) {
			if(chans != nil)
				imgerror("Jpeg baseline has > 1 scan");
			chans = jpegbaselinescan(is);
		}
		else
			jpegprogressivescan(is);
	}
	if(h.mode == byte SOF2)
		chans = jprogressiveIDCT(is);
	if(chans == nil)
		imgerror("jpeg has no image");
	width := is.width;
	height := is.height;
	if(width != h.X || height != h.Y) {
		for(k := 0; k < len chans; k++)
			chans[k] = resample(chans[k], h.X, h.Y, width, height);
	}
	if(dbgev)
		CU->event("IMAGE_JPG_REMAP", 0);
	if(len chans == 1)
		remapgrey(chans[0], width, height);
	else
		remaprgb(chans, width, height);
	if(dbgev)
		CU->event("IMAGE_JPG_REMAP_END", 0);
	im := newimage(width, height);
	im.writepixels(im.r, chans[0]);
	if(dbgev)
		CU->event("IMAGE_GETJPGMIM_END", 0);
	return newmi(im);
}

zig := array[64] of {
	0, 1, 8, 16, 9, 2, 3, 10, 17, # 0-7
	24, 32, 25, 18, 11, 4, 5, # 8-15
	12, 19, 26, 33, 40, 48, 41, 34, # 16-23
	27, 20, 13, 6, 7, 14, 21, 28, # 24-31
	35, 42, 49, 56, 57, 50, 43, 36, # 32-39
	29, 22, 15, 23, 30, 37, 44, 51, # 40-47
	58, 59, 52, 45, 38, 31, 39, 46, # 48-55
	53, 60, 61, 54, 47, 55, 62, 63 # 56-63
};

jpegbaselinescan(is: ref ImageSource) : array of array of byte
{
	if(dbg)
		sys->print("jpegbaselinescan\n");
	if(dbgev)
		CU->event("IMAGE_JPGBASELINESCAN", 0);
	h := is.jstate;
	Ns := h.Ns;
	if(Ns != h.Nf)
		imgerror("Jpeg baseline needs Ns==Nf");
	if(!(Ns==3 || Ns==1))
		imgerror("Jpeg baseline needs Ns==1 or 3");

	res := ResourceState.cur();
	heapavail := res.heaplim - res.heap;

	# check heap availability for
	#   chans: (3+Ns)*4 + (Ns*(3*4+h.X*h.Y)) bytes
	#   Td, Ta, data, H, V, DC: 6 arrays of (3+Ns)*4 bytes
	#
	heapavail -= (3+Ns)*28 + (Ns*(12 + h.X * h.Y));
	if(heapavail <= 0) {
		if(dbg)
			sys->print("jpegbaselinescan: no memory for chans et al.\n");
		imgerror("not enough memory");
	}
	
	chans := array[h.Nf] of array of byte;
	for(k:=0; k<h.Nf; k++)
		chans[k] = array[h.X*h.Y] of byte;

	# build per-component arrays
	Td := array[Ns] of int;
	Ta := array[Ns] of int;
	data := array[Ns] of array of array of int;
	H := array[Ns] of int;
	V := array[Ns] of int;
	DC := array[Ns] of int;

	# compute maximum H and V
	Hmax := 0;
	Vmax := 0;
	for(comp:=0; comp<Ns; comp++) {
		if(h.comp[comp].H > Hmax)
			Hmax = h.comp[comp].H;
		if(h.comp[comp].V > Vmax)
			Vmax = h.comp[comp].V;
	}
	if(dbg > 1)
		sys->print("Hmax=%d, Vmax=%d\n", Hmax, Vmax);

	# initialize data structures
	allHV1 := 1;
	for(comp=0; comp<Ns; comp++) {
		# JPEG requires scan components to be in same order as in frame,
		# so if both have 3 we know scan is Y Cb Cr and there's no need to
		# reorder
		Td[comp] = h.scomp[comp].tdc;
		Ta[comp] = h.scomp[comp].tac;
		H[comp] = h.comp[comp].H;
		V[comp] = h.comp[comp].V;
		nblock := H[comp]*V[comp];
		if(nblock != 1)
			allHV1 = 0;

		# data[comp]: needs (3+nblock)*4 + nblock*(3+8*8)*4 bytes
		heapavail -= 272*nblock + 12;
		if(heapavail <= 0){
			if(dbg)
				sys->print("jpegbaselinescan: no memory for data\n");
			imgerror("not enough memory");
		}

		data[comp] = array[nblock] of array of int;
		DC[comp] = 0;
		for(m:=0; m<nblock; m++)
			data[comp][m] = array[8*8] of int;
		if(dbg > 2)
			sys->print("scan comp %d: H=%d, V=%d, nblock=%d, Td=%d, Ta=%d\n",
				comp, H[comp], V[comp], nblock, Td[comp], Ta[comp]);
	}

	ri := h.ri;

	h.cnt = 0;
	h.sr = 0;
	nacross := ((h.X+(8*Hmax-1))/(8*Hmax));
	nmcu := ((h.Y+(8*Vmax-1))/(8*Vmax))*nacross;
	if(dbg)
		sys->print("nacross=%d, nmcu=%d\n", nacross, nmcu);
	for(mcu:=0; mcu<nmcu; ) {
		if(dbg > 2)
			sys->print("mcu %d\n", mcu);
		for(comp=0; comp<Ns; comp++) {
			if(dbg > 2)
				sys->print("comp %d\n", comp);
			dcht := h.dcht[Td[comp]];
			acht := h.acht[Ta[comp]];
			qt := h.qt[h.comp[comp].Tq];

			for(block:=0; block<H[comp]*V[comp]; block++) {
				if(dbg > 2)
					sys->print("block %d\n", block);
				# F-22
				t := jdecode(is, dcht);
				diff := jreceive(is, t);
				DC[comp] += diff;
				if(dbg > 2)
					sys->print("t=%d, diff=%d, DC=%d\n", t, diff, DC[comp]);

				# F-23
				zz := data[comp][block];
				zz[0:] = zeroints;
				zz[0] = qt[0]*DC[comp];
				k = 1;

				for(;;) {
					rs := jdecode(is, acht);
					(rrrr, ssss) := nibbles(rs);
					if(ssss == 0){
						if(rrrr != 15)
							break;
						k += 16;
					}else{
						k += rrrr;
						z := jreceive(is, ssss);
						zz[zig[k]] = z*qt[k];
						if(k == 63)
							break;
						k++;
					}
				}

				idct(zz);
			}
		}

		# rotate colors to RGB and assign to bytes
		if(Ns == 1) # very easy
			colormap1(h, chans[0], data[0][0], mcu, nacross);
		else if(allHV1) # fairly easy
			colormapall1(h, chans, data[0][0], data[1][0], data[2][0], mcu, nacross);
		else # miserable general case
			colormap(h, chans, data[0], data[1], data[2], mcu, nacross, Hmax, Vmax, H, V);

		# process restart marker, if present
		mcu++;
		if(ri>0 && mcu<nmcu && mcu%ri==0){
			jrestart(is, mcu);
			for(comp=0; comp<Ns; comp++)
				DC[comp] = 0;
		}
	}
	if(dbgev)
		CU->event("IMAGE_JPGBASELINESCAN_END", 0);
	return chans;
}

jrestart(is: ref ImageSource, mcu: int)
{
	h := is.jstate;
	ri := h.ri;
	restart := mcu/ri-1;
	rst, nskip: int;
	nskip = 0;
	do {
		do{
			rst = jnextborm(is);
			nskip++;
		}while(rst>=0 && rst!=16rFF);
		if(rst == 16rFF){
			rst = jnextborm(is);
			nskip++;
		}
	} while(rst>=0 && (rst&~7)!= RST);
	if(nskip != 2 || rst < 0 || ((rst&7) != (restart&7)))
		imgerror("Jpeg restart problem");
	h.cnt = 0;
	h.sr = 0;
}

jpegprogressivescan(is: ref ImageSource)
{
	if(dbgev)
		CU->event("IMAGE_JPGPROGSCAN", 0);
	h := is.jstate;
	if(h.dccoeff == nil)
		jprogressiveinit(h);

	c := h.scomp[0].C;
	comp := -1;
	for(i:=0; i<h.Nf; i++)
		if(h.comp[i].C == c)
			comp = i;
	if(comp == -1)
		imgerror("Jpeg bad component index in scan header");

	if(h.Ss == 0)
		jprogressivedc(is, comp);
	else if(h.Ah == 0)
		jprogressiveac(is, comp);
	else
		jprogressiveacinc(is, comp);
	if(dbgev)
		CU->event("IMAGE_JPGPROGSCAN_END", 0);
}

jprogressiveIDCT(is: ref ImageSource): array of array of byte
{
	if(dbgev)
		CU->event("IMAGE_JPGPROGIDCT", 0);
	h := is.jstate;
	Nf := h.Nf;

	res := ResourceState.cur();
	heapavail := res.heaplim - res.heap;

	# check heap availability for
	#   H, V, data, blockno: 4 arrays of (3+Nf)*4 bytes
	#   chans: (3+Nf)*4 + (Nf*(3*4+h.X*h.Y)) bytes
	#
	heapavail -= (3+Nf)*20 + (Nf*(12 + h.X * h.Y));
	if(heapavail <= 0) {
		if(dbg)
			sys->print("jprogressiveIDCT: no memory for chans et al.\n");
		imgerror("not enough memory");
	}
	H := array[Nf] of int;
	V := array[Nf] of int;

	allHV1 := 1;

	data := array[Nf] of array of array of int;
	for(comp:=0; comp<Nf; comp++){
		H[comp] = h.comp[comp].H;
		V[comp] = h.comp[comp].V;
		nblock := h.nblock[comp];
		if(nblock != 1)
			allHV1 = 0;

		# data[comp]: needs (3+nblock)*4 + nblock*(3+8*8)*4 bytes
		heapavail -= 272*nblock + 12;
		if(heapavail <= 0){
			if(dbg)
				sys->print("jprogressiveIDCT: no memory for data\n");
			imgerror("not enough memory");
		}

		data[comp] = array[nblock] of array of int;
		for(m:=0; m<nblock; m++)
			data[comp][m] = array[8*8] of int;
	}

	chans := array[h.Nf] of array of byte;
	for(k:=0; k<h.Nf; k++)
		chans[k] = array[h.X*h.Y] of byte;

	blockno := array[Nf] of {* => 0};
	nmcu := h.nacross*h.ndown;
	for(mcu:=0; mcu<nmcu; mcu++){
		for(comp=0; comp<Nf; comp++){
			dccoeff := h.dccoeff[comp];
			accoeff := h.accoeff[comp];
			bn := blockno[comp];
			for(block:=0; block<h.nblock[comp]; block++){
				zz := data[comp][block];
				zz[0:] = zeroints;
				zz[0] = dccoeff[bn];

				for(k=1; k<64; k++)
					zz[zig[k]] = accoeff[bn][k];

				idct(zz);
				bn++;
			}
			blockno[comp] = bn;
		}

		# rotate colors to RGB and assign to bytes
		if(Nf == 1) # very easy
			colormap1(h, chans[0], data[0][0], mcu, h.nacross);
		else if(allHV1) # fairly easy
			colormapall1(h, chans, data[0][0], data[1][0], data[2][0], mcu, h.nacross);
		else # miserable general case
			colormap(h, chans, data[0], data[1], data[2], mcu, h.nacross, h.Hmax, h.Vmax, H, V);
	}
	return chans;
}

jprogressiveinit(h: ref Jpegstate)
{
	Ns := h.Ns;
	Nf := h.Nf;
	if((Ns!=3 && Ns!=1) || Ns!=Nf)
		imgerror("Jpeg image must have 1 or 3 components");

	# compute maximum H and V
	h.Hmax = 0;
	h.Vmax = 0;
	for(comp:=0; comp<Nf; comp++){
		if(h.comp[comp].H > h.Hmax)
			h.Hmax = h.comp[comp].H;
		if(h.comp[comp].V > h.Vmax)
			h.Vmax = h.comp[comp].V;
	}
	h.nacross = ((h.X+(8*h.Hmax-1))/(8*h.Hmax));
	h.ndown = ((h.Y+(8*h.Vmax-1))/(8*h.Vmax));
	nmcu := h.nacross*h.ndown;

	res := ResourceState.cur();
	heapavail := res.heaplim - res.heap;

	# check heap availability for
	#   h.dccoeff: (3+Nf)*4 bytes
	#   h.accoeff: (3+Nf)*4 bytes
	heapavail -= (3+Nf)*8;
	if(heapavail <= 0) {
		if(dbg)
			sys->print("jprogressiveinit: no memory for coeffs\n");
		imgerror("not enough memory");
	}

	h.dccoeff = array[Nf] of array of int;
	h.accoeff = array[Nf] of array of array of int;
	for(k:=0; k<Nf; k++){
		n := h.nblock[k]*nmcu;

		# check heap availability for
		#   h.dccoeff[k]: (3+n)*4 bytes
		#   h.accoeff[k]: (3+n)*4 + n*(3+64)*4 bytes
		heapavail -= 276*n + 24;
		if(heapavail <= 0){
			if(dbg)
				sys->print("jprogressiveinit: no memory for coeff arrays\n");
			imgerror("not enough memory");
		}

		h.dccoeff[k] = array[n] of {* => 0};
		h.accoeff[k] = array[n] of array of int;
		for(j:=0; j<n; j++)
			h.accoeff[k][j] = array[64] of {* => 0};
	}
}

jprogressivedc(is: ref ImageSource, comp: int)
{
	h := is.jstate;
	Ns := h.Ns;
	Ah := h.Ah;
	Al := h.Al;
	if(Ns!=h.Nf)
		imgerror("Jpeg progressive with Nf!=Ns in DC scan");

	# build per-component arrays
	Td := array[Ns] of int;
	DC := array[Ns] of int;

	# initialize data structures
	h.cnt = 0;
	h.sr = 0;
	for(comp=0; comp<Ns; comp++) {
		# JPEG requires scan components to be in same order as in frame,
		# so if both have 3 we know scan is Y Cb Cr and there's no need to
		# reorder
		Td[comp] = h.scomp[comp].tdc;
		DC[comp] = 0;
	}

	ri := h.ri;

	nmcu := h.nacross*h.ndown;
	blockno := array[Ns] of {* => 0};
	for(mcu:=0; mcu<nmcu; ){
		for(comp=0; comp<Ns; comp++){
			dcht := h.dcht[Td[comp]];
			qt := h.qt[h.comp[comp].Tq][0];
			dc := h.dccoeff[comp];
			bn := blockno[comp];

			for(block:=0; block<h.nblock[comp]; block++) {
				if(Ah == 0) {
					t := jdecode(is, dcht);
					diff := jreceive(is, t);
					DC[comp] += diff;
					dc[bn] = qt*DC[comp]<<Al;
				} else
					dc[bn] |= qt*jreceivebit(is)<<Al;
				bn++;
			}
			blockno[comp] = bn;
		}

		# process restart marker, if present
		mcu++;
		if(ri>0 && mcu<nmcu && mcu%ri==0){
			jrestart(is, mcu);
			for(comp=0; comp<Ns; comp++)
				DC[comp] = 0;
		}
	}
}

jprogressiveac(is: ref ImageSource, comp: int)
{
	h := is.jstate;
	Ns := h.Ns;
	Al := h.Al;
	if(Ns != 1)
		imgerror("Jpeg illegal Ns>1 in progressive AC scan");
	Ss := h.Ss;
	Se := h.Se;
	H := h.comp[comp].H;
	V := h.comp[comp].V;

	nacross := h.nacross*H;
	ndown := h.ndown*V;
	q := 8*h.Hmax/H;
	nhor := (h.X+q-1)/q;
	q = 8*h.Vmax/V;
	nver := (h.Y+q-1)/q;

	# initialize data structures
	h.cnt = 0;
	h.sr = 0;
	Ta := h.scomp[0].tac;

	ri := h.ri;

	eobrun := 0;
	acht := h.acht[Ta];
	qt := h.qt[h.comp[comp].Tq];
	nmcu := nacross*ndown;
	mcu := 0;
	for(y:=0; y<nver; y++) {
		for(x:=0; x<nhor; x++) {
			# Figure G-3
			if(eobrun > 0){
				--eobrun;
				continue;
			}

			# arrange blockno to be in same sequence as
			# original scan calculation.
			tmcu := x/H + (nacross/H)*(y/V);
			blockno := tmcu*H*V + H*(y%V) + x%H;
			acc := h.accoeff[comp][blockno];
			k := Ss;
			for(;;) {
				rs := jdecode(is, acht);
				(rrrr, ssss) := nibbles(rs);
				if(ssss == 0) {
					if(rrrr < 15) {
						eobrun = 0;
						if(rrrr > 0)
							eobrun = jreceiveEOB(is, rrrr)-1;
						break;
					}
					k += 16;
				}
				else {
					k += rrrr;
					z := jreceive(is, ssss);
					acc[k] = z*qt[k]<<Al;
					if(k == Se)
						break;
					k++;
				}
			}
		}

		# process restart marker, if present
		mcu++;
		if(ri>0 && mcu<nmcu && mcu%ri==0) {
			jrestart(is, mcu);
			eobrun = 0;
		}
	}
}

jprogressiveacinc(is: ref ImageSource, comp: int)
{
	h := is.jstate;
	Ns := h.Ns;
	if(Ns != 1)
		imgerror("Jpeg  illegal Ns>1 in progressive AC scan");
	Ss := h.Ss;
	Se := h.Se;
	H := h.comp[comp].H;
	V := h.comp[comp].V;
	Al := h.Al;

	nacross := h.nacross*H;
	ndown := h.ndown*V;
	q := 8*h.Hmax/H;
	nhor := (h.X+q-1)/q;
	q = 8*h.Vmax/V;
	nver := (h.Y+q-1)/q;

	# initialize data structures
	h.cnt = 0;
	h.sr = 0;
	Ta := h.scomp[0].tac;
	ri := h.ri;

	eobrun := 0;
	ac := h.accoeff[comp];
	acht := h.acht[Ta];
	qt := h.qt[h.comp[comp].Tq];
	nmcu := nacross*ndown;
	mcu := 0;
	pending := 0;
	nzeros := -1;
	for(y:=0; y<nver; y++){
		for(x:=0; x<nhor; x++){
			# Figure G-7

			# arrange blockno to be in same sequence as
			# original scan calculation.
			tmcu := x/H + (nacross/H)*(y/V);
			blockno := tmcu*H*V + H*(y%V) + x%H;
			acc := ac[blockno];
			if(eobrun > 0){
				if(nzeros > 0)
					imgerror("Jpeg zeros pending at block start");
				for(k:=Ss; k<=Se; k++)
					jincrement(is, acc, k, qt[k]<<Al);
				--eobrun;
				continue;
			}

			for(k:=Ss; k<=Se; ){
				if(nzeros >= 0){
					if(acc[k] != 0)
						jincrement(is, acc, k, qt[k]<<Al);
					else if(nzeros-- == 0)
						acc[k] = pending;
					k++;
					continue;
				}
				rs := jdecode(is, acht);
				(rrrr, ssss) := nibbles(rs);
				if(ssss == 0){
					if(rrrr < 15){
						eobrun = 0;
						if(rrrr > 0)
							eobrun = jreceiveEOB(is, rrrr)-1;
						while(k <= Se){
							jincrement(is, acc, k, qt[k]<<Al);
							k++;
						}
						break;
					}
					for(i:=0; i<16; k++){
						jincrement(is, acc, k, qt[k]<<Al);
						if(acc[k] == 0)
							i++;
					}
					continue;
				}else if(ssss != 1)
					imgerror("Jpeg ssss!=1 in progressive increment");
				nzeros = rrrr;
				pending = jreceivebit(is);
				if(pending == 0)
					pending = -1;
				pending *= qt[k]<<Al;
			}
		}

		# process restart marker, if present
		mcu++;
		if(ri>0 && mcu<nmcu && mcu%ri==0){
			jrestart(is, mcu);
			eobrun = 0;
			nzeros = -1;
		}
	}
}

jincrement(is: ref ImageSource, acc: array of int, k, Pt: int)
{
	if(acc[k] == 0)
		return;
	b := jreceivebit(is);
	if(b != 0)
		if(acc[k] < 0)
			acc[k] -= Pt;
		else
			acc[k] += Pt;
}

jc1: con 2871;		# 1.402 * 2048
jc2: con 705;		# 0.34414 * 2048
jc3: con 1463;		# 0.71414 * 2048
jc4: con 3629;		# 1.772 * 2048

# Fills in pixels (x,y) for x = minx=8*(mcu%nacross), minx+1, ..., minx+7 (or h.X-1, if less)
# and for y = miny=8*(mcu/nacross), miny+1, ..., miny+7 (or h.Y-1, if less)
colormap1(h: ref Jpegstate, pic: array of byte, data: array of int, mcu, nacross: int)
{
	minx := 8*(mcu%nacross);
	dx := 8;
	if(minx+dx > h.X)
		dx = h.X-minx;
	miny := 8*(mcu/nacross);
	dy := 8;
	if(miny+dy > h.Y)
		dy = h.Y-miny;
	pici := miny*h.X+minx;
	k := 0;
	for(y:=0; y<dy; y++) {
		for(x:=0; x<dx; x++)
			pic[pici+x] = clampb[(data[k+x]+128)+CLAMPBOFF];
		pici += h.X;
		k += 8;
	}
}

# Fills in same pixels as colormap1
colormapall1(h: ref Jpegstate, chans: array of array of byte, data0, data1, data2: array of int, mcu, nacross: int)
{
	rpic := chans[0];
	gpic := chans[1];
	bpic := chans[2];
	minx := 8*(mcu%nacross);
	dx := 8;
	if(minx+dx > h.X)
		dx = h.X-minx;
	miny := 8*(mcu/nacross);
	dy := 8;
	if(miny+dy > h.Y)
		dy = h.Y-miny;
	pici := miny*h.X+minx;
	k := 0;
	for(y:=0; y<dy; y++) {
		for(x:=0; x<dx; x++){
			if(jpegcolorspace == CYCbCr) {
				rpic[pici+x] = clampb[data0[k+x]+128+CLAMPBOFF];
				gpic[pici+x] = clampb[data1[k+x]+128+CLAMPBOFF];
				bpic[pici+x] = clampb[data2[k+x]+128+CLAMPBOFF];
			}
			else { # RGB
				Y := (data0[k+x]+128) << 11;
				Cb := data1[k+x];
				Cr := data2[k+x];
				r := Y+jc1*Cr;
				g := Y-jc2*Cb-jc3*Cr;
				b := Y+jc4*Cb;
				rpic[pici+x] = clampb[(r>>11)+CLAMPBOFF];
				gpic[pici+x] = clampb[(g>>11)+CLAMPBOFF];
				bpic[pici+x] = clampb[(b>>11)+CLAMPBOFF];
			}
		}
		pici += h.X;
		k += 8;
	}
}

# Fills in pixels (x,y) for x = minx=8*Hmax*(mcu%nacross), minx+1, ..., minx+8*Hmax-1 (or h.X-1, if less)
# and for y = miny=8*Vmax*(mcu/nacross), miny+1, ..., miny+8*Vmax-1 (or h.Y-1, if less)
colormap(h: ref Jpegstate, chans: array of array of byte, data0, data1, data2: array of array of int, mcu, nacross, Hmax, Vmax: int,  H, V: array of int)
{
	rpic := chans[0];
	gpic := chans[1];
	bpic := chans[2];
	minx := 8*Hmax*(mcu%nacross);
	dx := 8*Hmax;
	if(minx+dx > h.X)
		dx = h.X-minx;
	miny := 8*Vmax*(mcu/nacross);
	dy := 8*Vmax;
	if(miny+dy > h.Y)
		dy = h.Y-miny;
	pici := miny*h.X+minx;
	H0 := H[0];
	H1 := H[1];
	H2 := H[2];
	if(dbg > 2)
		sys->print("colormap, minx=%d, miny=%d, dx=%d, dy=%d, pici=%d, H0=%d, H1=%d, H2=%d\n",
			minx, miny, dx, dy, pici, H0, H1, H2);
	for(y:=0; y<dy; y++) {
		t := y*V[0];
		b0 := H0*(t/(8*Vmax));
		y0 := 8*((t/Vmax)&7);
		t = y*V[1];
		b1 := H1*(t/(8*Vmax));
		y1 := 8*((t/Vmax)&7);
		t = y*V[2];
		b2 := H2*(t/(8*Vmax));
		y2 := 8*((t/Vmax)&7);
		x0 := 0;
		x1 := 0;
		x2 := 0;
		for(x:=0; x<dx; x++) {
			if(jpegcolorspace == CYCbCr) {
				rpic[pici+x] = clampb[data0[b0][y0+x0++*H0/Hmax] + 128 + CLAMPBOFF];
				gpic[pici+x] = clampb[data1[b1][y1+x1++*H1/Hmax] + 128 + CLAMPBOFF];
				bpic[pici+x] = clampb[data2[b2][y2+x2++*H2/Hmax] + 128 + CLAMPBOFF];
			}
			else { # RGB
				Y := (data0[b0][y0+x0++*H0/Hmax]+128) << 11;
				Cb := data1[b1][y1+x1++*H1/Hmax];
				Cr := data2[b2][y2+x2++*H2/Hmax];
				r := Y+jc1*Cr;
				g := Y-jc2*Cb-jc3*Cr;
				b := Y+jc4*Cb;
				rpic[pici+x] = clampb[(r>>11)+CLAMPBOFF];
				gpic[pici+x] = clampb[(g>>11)+CLAMPBOFF];
				bpic[pici+x] = clampb[(b>>11)+CLAMPBOFF];
			}
			if(x0*H0/Hmax >= 8){
				x0 = 0;
				b0++;
			}
			if(x1*H1/Hmax >= 8){
				x1 = 0;
				b1++;
			}
			if(x2*H2/Hmax >= 8){
				x2 = 0;
				b2++;
			}
		}
		pici += h.X;
	}
}

# decode next 8-bit value from entropy-coded input.  chart F-26
jdecode(is: ref ImageSource, t: ref Huffman): int
{
	h := is.jstate;
	maxcode := t.maxcode;
	if(h.cnt < 8)
		jnextbyte(is);
	# fast lookup
	code := (h.sr>>(h.cnt-8))&16rFF;
	v := t.value[code];
	if(v >= 0){
		h.cnt -= t.shift[code];
		return v;
	}

	h.cnt -= 8;
	if(h.cnt == 0)
		jnextbyte(is);
	h.cnt--;
	cnt := h.cnt;
	m := 1<<cnt;
	sr := h.sr;
	code <<= 1;
	i := 9;
	for(;;i++){
		if(sr & m)
			code |= 1;
		if(code <= maxcode[i])
			break;
		code <<= 1;
		m >>= 1;
		if(m == 0){
			sr = jnextbyte(is);
			m = 16r80;
			cnt = 8;
		}
		cnt--;
	}
	h.cnt = cnt;
	return t.val[t.valptr[i]+(code-t.mincode[i])];
}

# load next byte of input
jnextbyte(is: ref ImageSource): int
{
	b :=getc(is);

	if(b == 16rFF) {
		b2 :=getc(is);
		if(b2 != 0) {
			if(b2 == int DNL)
				imgerror("Jpeg  DNL marker unimplemented");
			# decoder is reading into marker; satisfy it and restore state
			ungetc2(is, byte b);
		}
	}
	h := is.jstate;
	h.cnt += 8;
	h.sr = (h.sr<<8)| b;
	return b;
}

# like jnextbyte, but look for marker too
jnextborm(is: ref ImageSource): int
{
	b :=getc(is);

	if(b == 16rFF)
		return b;
	h := is.jstate;
	h.cnt += 8;
	h.sr = (h.sr<<8)| b;
	return b;
}

# return next s bits of input, MSB first, and level shift it
jreceive(is: ref ImageSource, s: int): int
{
	h := is.jstate;
	while(h.cnt < s)
		jnextbyte(is);
	h.cnt -= s;
	v := h.sr >> h.cnt;
	m := (1<<s);
	v &= m-1;
	# level shift
	if(v < (m>>1))
		v += ~(m-1)+1;
	return v;
}

# return next s bits of input, decode as EOB
jreceiveEOB(is: ref ImageSource, s: int): int
{
	h := is.jstate;
	while(h.cnt < s)
		jnextbyte(is);
	h.cnt -= s;
	v := h.sr >> h.cnt;
	m := (1<<s);
	v &= m-1;
	# level shift
	v += m;
	return v;
}

# return next bit of input
jreceivebit(is: ref ImageSource): int
{
	h := is.jstate;
	if(h.cnt < 1)
		jnextbyte(is);
	h.cnt--;
	return (h.sr >> h.cnt) & 1;
}


nibbles(c: int) : (int, int)
{
	return (c>>4, c&15);

}

# Scaled integer implementation.
# inverse two dimensional DCT, Chen-Wang algorithm
# (IEEE ASSP-32, pp. 803-816, Aug. 1984)
# 32-bit integer arithmetic (8 bit coefficients)
# 11 mults, 29 adds per DCT
#
# coefficients extended to 12 bit for IEEE1180-1990
# compliance

W1:	con 2841;	# 2048*sqrt(2)*cos(1*pi/16)
W2:	con 2676;	# 2048*sqrt(2)*cos(2*pi/16)
W3:	con 2408;	# 2048*sqrt(2)*cos(3*pi/16)
W5:	con 1609;	# 2048*sqrt(2)*cos(5*pi/16)
W6:	con 1108;	# 2048*sqrt(2)*cos(6*pi/16)
W7:	con 565;	# 2048*sqrt(2)*cos(7*pi/16)

W1pW7:	con 3406;	# W1+W7
W1mW7:	con 2276;	# W1-W7
W3pW5:	con 4017;	# W3+W5
W3mW5:	con 799;	# W3-W5
W2pW6:	con 3784;	# W2+W6
W2mW6:	con 1567;	# W2-W6

R2:	con 181;	# 256/sqrt(2)

idct(b: array of int)
{
	# transform horizontally
	for(y:=0; y<8; y++){
		eighty := y<<3;
		# if all non-DC components are zero, just propagate the DC term
		if(b[eighty+1]==0)
		if(b[eighty+2]==0 && b[eighty+3]==0)
		if(b[eighty+4]==0 && b[eighty+5]==0)
		if(b[eighty+6]==0 && b[eighty+7]==0){
			v := b[eighty]<<3;
			b[eighty+0] = v;
			b[eighty+1] = v;
			b[eighty+2] = v;
			b[eighty+3] = v;
			b[eighty+4] = v;
			b[eighty+5] = v;
			b[eighty+6] = v;
			b[eighty+7] = v;
			continue;
		}
		# prescale
		x0 := (b[eighty+0]<<11)+128;
		x1 := b[eighty+4]<<11;
		x2 := b[eighty+6];
		x3 := b[eighty+2];
		x4 := b[eighty+1];
		x5 := b[eighty+7];
		x6 := b[eighty+5];
		x7 := b[eighty+3];
		# first stage
		x8 := W7*(x4+x5);
		x4 = x8 + W1mW7*x4;
		x5 = x8 - W1pW7*x5;
		x8 = W3*(x6+x7);
		x6 = x8 - W3mW5*x6;
		x7 = x8 - W3pW5*x7;
		# second stage
		x8 = x0 + x1;
		x0 -= x1;
		x1 = W6*(x3+x2);
		x2 = x1 - W2pW6*x2;
		x3 = x1 + W2mW6*x3;
		x1 = x4 + x6;
		x4 -= x6;
		x6 = x5 + x7;
		x5 -= x7;
		# third stage
		x7 = x8 + x3;
		x8 -= x3;
		x3 = x0 + x2;
		x0 -= x2;
		x2 = (R2*(x4+x5)+128)>>8;
		x4 = (R2*(x4-x5)+128)>>8;
		# fourth stage
		b[eighty+0] = (x7+x1)>>8;
		b[eighty+1] = (x3+x2)>>8;
		b[eighty+2] = (x0+x4)>>8;
		b[eighty+3] = (x8+x6)>>8;
		b[eighty+4] = (x8-x6)>>8;
		b[eighty+5] = (x0-x4)>>8;
		b[eighty+6] = (x3-x2)>>8;
		b[eighty+7] = (x7-x1)>>8;
	}
	# transform vertically
	for(x:=0; x<8; x++){
		# if all non-DC components are zero, just propagate the DC term
		if(b[x+8*1]==0)
		if(b[x+8*2]==0 && b[x+8*3]==0)
		if(b[x+8*4]==0 && b[x+8*5]==0)
		if(b[x+8*6]==0 && b[x+8*7]==0){
			v := (b[x+8*0]+32)>>6;
			b[x+8*0] = v;
			b[x+8*1] = v;
			b[x+8*2] = v;
			b[x+8*3] = v;
			b[x+8*4] = v;
			b[x+8*5] = v;
			b[x+8*6] = v;
			b[x+8*7] = v;
			continue;
		}
		# prescale
		x0 := (b[x+8*0]<<8)+8192;
		x1 := b[x+8*4]<<8;
		x2 := b[x+8*6];
		x3 := b[x+8*2];
		x4 := b[x+8*1];
		x5 := b[x+8*7];
		x6 := b[x+8*5];
		x7 := b[x+8*3];
		# first stage
		x8 := W7*(x4+x5) + 4;
		x4 = (x8+W1mW7*x4)>>3;
		x5 = (x8-W1pW7*x5)>>3;
		x8 = W3*(x6+x7) + 4;
		x6 = (x8-W3mW5*x6)>>3;
		x7 = (x8-W3pW5*x7)>>3;
		# second stage
		x8 = x0 + x1;
		x0 -= x1;
		x1 = W6*(x3+x2) + 4;
		x2 = (x1-W2pW6*x2)>>3;
		x3 = (x1+W2mW6*x3)>>3;
		x1 = x4 + x6;
		x4 -= x6;
		x6 = x5 + x7;
		x5 -= x7;
		# third stage
		x7 = x8 + x3;
		x8 -= x3;
		x3 = x0 + x2;
		x0 -= x2;
		x2 = (R2*(x4+x5)+128)>>8;
		x4 = (R2*(x4-x5)+128)>>8;
		# fourth stage
		b[x+8*0] = (x7+x1)>>14;
		b[x+8*1] = (x3+x2)>>14;
		b[x+8*2] = (x0+x4)>>14;
		b[x+8*3] = (x8+x6)>>14;
		b[x+8*4] = (x8-x6)>>14;
		b[x+8*5] = (x0-x4)>>14;
		b[x+8*6] = (x3-x2)>>14;
		b[x+8*7] = (x7-x1)>>14;
	}
}

################# Remap colors and Dither ##############

closest_rgbpix(r, g, b: int) : int
{
	pix := int closestrgb[((r>>4)<<8)+((g>>4)<<4)+(b>>4)];
	# If white is the closest but original r,g,b wasn't white,
	# look for another color, because web page designer probably
	# cares more about contrast than actual color
	if(pix == 0 && !(r == 255 && g ==255 && b == 255)) {
		bestdist := 1000000;
		for(i := 1; i < 256; i++) {
			dr := r-rgbvmap_r[i];
			dg := g-rgbvmap_g[i];
			db := b-rgbvmap_b[i];
			d := dr*dr + dg*dg + db*db;
			if(d < bestdist) {
				bestdist = d;
				pix = i;
			}
		}
	}
	return pix;
}

CLAMPBOFF: con 300;
NCLAMPB: con CLAMPBOFF+256+CLAMPBOFF;
CLAMPNOFF: con 64;
NCLAMPN: con CLAMPNOFF+256+CLAMPNOFF;

clampb: array of byte;		# clamps byte values
clampn_b: array of int;		# clamps byte values, then shifts >> 4
clampn_g: array of int;		# clamps byte values, then masks off lower 4 bits
clampn_r: array of int;		# clamps byte values, masks off lower 4 bits, then shifts <<4

init_tabs()
{
	clampn_b = array[NCLAMPN] of int;
	clampn_g = array[NCLAMPN] of int;
	clampn_r = array[NCLAMPN] of int;
	for(j:=0; j<CLAMPNOFF; j++) {
		clampn_b[j] = 0;
		clampn_g[j] = 0;
		clampn_r[j] = 0;
	}
	for(j=0; j<256; j++) {
		t := j>>4;
		clampn_b[CLAMPNOFF+j] = t;
		clampn_g[CLAMPNOFF+j] = t<<4;
		clampn_r[CLAMPNOFF+j] = t<<8;
	}
	for(j=0; j<CLAMPNOFF; j++) {
		clampn_b[CLAMPNOFF+256+j] = 16r0F;
		clampn_g[CLAMPNOFF+256+j] = 16rF0;
		clampn_r[CLAMPNOFF+256+j] = 16rF00;
	}
	clampb = array[NCLAMPB] of byte;
	for(j=0; j<CLAMPBOFF; j++)
		clampb[j] = byte 0;
	for(j=0; j<256; j++)
		clampb[CLAMPBOFF+j] = byte j;
	for(j=0; j<CLAMPBOFF; j++)
		clampb[CLAMPBOFF+256+j] = byte 16rFF;
}

# Remap pixels of pic[] into the closest colors in the rgbv map,
# and do error diffusion of the result.
# pic is a one-channel image whose rgb values are given by looking
# up values in cmap.
remap1(pic: array of byte, dx, dy: int, cmap: array of byte)
{
	if(dbg)
		sys->print("remap1, pic len %d, dx=%d, dy=%d\n", len pic, dx, dy);
	cmap_r := array[256] of int;
	cmap_g := array[256] of int;
	cmap_b := array[256] of int;
	i := 0;
	for(j := 0; j < 256 && i < len cmap; j++) {
		cmap_r[j] = int cmap[i++];
		cmap_g[j] = int cmap[i++];
		cmap_b[j] = int cmap[i++];
	}
	# in case input has bad indices
	for( ; j < 256; j++) {
		cmap_r[j] = 0;
		cmap_g[j] = 0;
		cmap_b[j] = 0;
	}
	# modified floyd steinberg, coefficients (1 0) 3/16, (0, 1) 3/16, (1, 1) 7/16
	ered := array[dx+1] of { * => 0 };
	egrn := array[dx+1] of int;
	eblu := array[dx+1] of int;
	egrn[0:] = ered;
	eblu[0:] = ered;
	p := 0;
	for(y:=0; y<dy; y++) {
		er := 0;
		eg := 0;
		eb := 0;
		for(x:=0; x<dx; ) {
			x1 := x+1;
			in := int pic[p];
			r := cmap_r[in]+ered[x];
			g := cmap_g[in]+egrn[x];
			b := cmap_b[in]+eblu[x];
			col := int (pic[p++] = closestrgb[clampn_r[r+CLAMPNOFF]
					+clampn_g[g+CLAMPNOFF]
					+clampn_b[b+CLAMPNOFF]]);

			r -= rgbvmap_r[col];
			t := (3*r)>>4;
			ered[x] = t+er;
			ered[x1] += t;
			er = r-3*t;

			g -= rgbvmap_g[col];
			t = (3*g)>>4;
			egrn[x] = t+eg;
			egrn[x1] += t;
			eg = g-3*t;

			b -= rgbvmap_b[col];
			t = (3*b)>>4;
			eblu[x] = t+eb;
			eblu[x1] += t;
			eb = b-3*t;

			x = x1;
		}
	}
}

# Remap pixels of pic[] into the closest greyscale colors in the rgbv map,
# and do error diffusion of the result.
# pic is a one-channel greyscale image.
remapgrey(pic: array of byte, dx, dy: int)
{
	if(dbg)
		sys->print("remapgrey, pic len %d, dx=%d, dy=%d\n", len pic, dx, dy);
	# modified floyd steinberg, coefficients (1 0) 3/16, (0, 1) 3/16, (1, 1) 7/16
	e := array[dx+1] of {* => 0 };
	p := 0;
	for(y:=0; y<dy; y++){
		eb := 0;
		for(x:=0; x<dx; ) {
			x1 := x+1;
			b := int pic[p]+e[x];
			b1 := clampn_b[b+CLAMPNOFF];
			col := 255-17*b1;
			pic[p++] = byte col;

			b -= rgbvmap_b[col];
			t := (3*b)>>4;
			e[x] = t+eb;
			e[x1] += t;
			eb = b-3*t;
			x = x1;
		}
	}
}

# Remap pixels of chans into the closest colors in the rgbv map,
# and do error diffusion of the result.
# chans is a 3-channel image whose channels are either (y,cb,cr) or
# (r,g,b), depending on whether jpegcolorspace is CYCbCr or CRGB.
# Variable names use r,g,b (historical).
# (right now we only call this routine from jpeg code).
remaprgb(chans: array of array of byte, dx, dy: int)
{
	if(dbg)
		sys->print("remaprgb, pic len %d, dx=%d, dy=%d\n", len chans[0], dx, dy);
	rpic := chans[0];
	gpic := chans[1];
	bpic := chans[2];
	pic := chans[0];
	# modified floyd steinberg, coefficients (1 0) 3/16, (0, 1) 3/16, (1, 1) 7/16
	ered := array[dx+1] of { * => 0 };
	egrn := array[dx+1] of int;
	eblu := array[dx+1] of int;
	egrn[0:] = ered;
	eblu[0:] = ered;
	closest: array of byte;
	map0, map1, map2: array of int;
	if(jpegcolorspace == CRGB) {
		closest = closestrgb;
		map0 = rgbvmap_r;
		map1 = rgbvmap_g;
		map2 = rgbvmap_b;
	}
	else {
		closest = closestycbcr;
		map0 = rgbvmap_y;
		map1 = rgbvmap_cb;
		map2 = rgbvmap_cr;
	}
	p := 0;
	for(y:=0; y<dy; y++ ) {
		er := 0;
		eg := 0;
		eb := 0;
		for(x:=0; x<dx; ) {
			x1 := x + 1;
			r := int rpic[p]+ered[x];
			g := int gpic[p]+egrn[x];
			b := int bpic[p]+eblu[x];
			# Errors can be uncorrectable if converting from YCbCr,
			# since we can't guarantee that an extremal value of one of
			# the components selects a color with an extremal value.
			# If we don't, the errors accumulate without bound.  This
			# doesn't happen in RGB because the closest table can guarantee
			# a color on the edge of the gamut, producing a zero error in
			# that component.  For the rotation YCbCr space, there may be
			# no color that can guarantee zero error at the edge.
			# Therefore we must clamp explicitly rather than by assuming
			# an upper error bound of CLAMPOFF.  The performance difference
			# is miniscule anyway.
			if(r < 0)
				r = 0;
			else if(r > 255)
				r = 255;
			if(g < 0)
				g = 0;
			else if(g > 255)
				g = 255;
			if(b < 0)
				b = 0;
			else if(b > 255)
				b = 255;
			col := int (pic[p++] = closest[(b>>4)+16*((g>>4)+16*(r>>4))]);

			r -= map0[col];
			t := (3*r)>>4;
			ered[x] = t+er;
			ered[x1] += t;
			er = r-3*t;

			g -= map1[col];
			t = (3*g)>>4;
			egrn[x] = t+eg;
			egrn[x1] += t;
			eg = g-3*t;

			b -= map2[col];
			t = (3*b)>>4;
			eblu[x] = t+eb;
			eblu[x1] += t;
			eb = b-3*t;

			x = x1;
		}
	}
}

# Given src array, representing sw*sh pixel values, resample them into
# the returned array, with dimensions dw*dh.
#
# Quick and dirty resampling: just interpolate.
# This lets us resample arrays of pixels indices (e.g., result of gif decoding).
# The filter-based resampling methods need conversion to rgb or grayscale.
# Also, although the results won't look good, people really shouldn't be
# asking the browser to resample except for special purposes (like the common
# case of resizing a 1x1 image to make a spacer).
resample(src: array of byte, sw, sh: int, dw, dh: int) : array of byte
{
	if(dbgev)
		CU->event("IMAGE_RESAMPLE_START", 0);
	if(src == nil || sw == 0 || sh == 0 || dw == 0 || dh == 0)
		return src;
	xfac := real sw / real dw;
	yfac := real sh / real dh;
	totpix := dw*dh;
	dst := array[totpix] of byte;
	dindex := 0;

	# precompute index in src row corresponding to each index in dst row
	sindices := array[dw] of int;
	for(dx := 0; dx < dw; dx++) {
		# Note: limbo rounds conversion of reals to ints
		sx := int (xfac * real dx);
		if(sx >= sw)
			sx = sw-1;
		sindices[dx] = sx;
	}
	for(dy := 0; dy < dh; dy++) {
		sy := int (yfac * real dy);
		if(sy >= sh)
			sy = sh-1;
		soffset := sy * sw;
		for(dx = 0; dx < dw; dx++)
			dst[dindex++] = src[soffset + sindices[dx]];
	}
	if(dbgev)
		CU->event("IMAGE_RESAMPLE_END", 0);
	return dst;
}

################# BIT ###################

getbitmim(is: ref ImageSource) : ref MaskedImage
{
	if(dbg)
		sys->print("img getbitmim: w=%d h=%d len=%d\n",
			is.width, is.height, len is.bs.data);

	im := getbitimage(display, is.bs.data);
	if(im == nil)
		imgerror("out of memory");
	is.i = is.bs.edata;		# getbitimage should do this too!
	is.width = im.r.max.x;
	is.height = im.r.max.y;
	return newmi(im);
}


NMATCH: con 3;			# shortest match possible
NCBLOCK: con 6000;		# size of compressed blocks

getbitimage(disp: ref Display, d: array of byte): ref Image
{
	compressed := 0;

	if(len d < 5*12)
		imgerror("bad bit format");

	if(string d[:11] == "compressed\n"){
		if(dbg)
			sys->print("img: bit compressed\n");
		compressed = 1;
		d = d[11:];
	}

	#check format of ldepth as a kind of magic number test
	for(m := 0; m < 10; m++)
		if(d[m] != byte ' ')
			imgerror("bad bit format");
	if(d[11] != byte ' ')
		imgerror("bad bit format");

	ld := int( d[10] - byte '0' );
	if(ld < 0 || ld > 3)
		imgerror("bad bit ldepth");

	xmin := int string d[ 1*12 : 2*12 ];
	ymin := int string d[ 2*12 : 3*12 ];
	xmax := int string d[ 3*12 : 4*12 ];
	ymax := int string d[ 4*12 : 5*12 ];
	if( (xmin > xmax) || (ymin > ymax) )
		imgerror("bad bit rectangle");

	if(dbg)
		sys->print("img: bit: ldepth=%d, xmin=%d, ymin=%d, xmax=%d, ymax=%d\n",
			ld, xmin, ymin, xmax, ymax);

	r := Rect( (xmin, ymin), (xmax, ymax) );
	im := disp.newimage(r, ld, 0, 0);
	if(im == nil)
		return nil;

	if (!compressed){
		im.writepixels(im.r, d[5*12:]);
		return im;
	}

	# see /image/readimage.c, /image/creadimage.c, and
	# /memimage/cload.c for reference implementation
	# of bit compression

	a := array[(xmax-xmin)*(ymax-ymin)*(2<<ld)] of byte;
	ai := 0;		#index into uncompressed data array a
	di := 5*12;		#index into compressed data
	while(ymin < ymax){
		y := int string d[ di        : di + 1*12 ];
		n := int string d[ di + 1*12 : di + 2*12 ];
		di += 2*12;

		if (y <= ymin || ymax < y)
			imgerror("bad compressed bit y-max");
		if (n <= 0 || NCBLOCK < n)
			imgerror("bad compressed bit count");

		# no input-stream error checking :-(
		u := di;
		while(di < u+n){
			c := int d[di++];
			if (c >= 128){
				# copy as is
				cnt := c-128 + 1;

				# check for overrun of index di within d?

				# a[ai:ai+cnt] = d[di:di+cnt];
				for(i:=0; i<cnt; i++)
					a[ai+i] = d[di+i];
				di += cnt;
				ai += cnt;
			}
			else {
				# copy a run/match
				offs := int(d[di++]) + ((c&3)<<8) + 1;
				cnt := (c>>2) + NMATCH;

				# simply: a[ai:ai+cnt] = a[ai-offs:ai-offs+cnt];
				for(i:=0; i<cnt; i++)
					a[ai+i] = a[ai-offs+i];
				ai += cnt;
			}
		}
		ymin = y;
	}
	im.writepixels(im.r, a);
	return im;
}

