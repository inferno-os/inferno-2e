implement RC4;

include "rc4.m";

setupRC4state(start: array of byte, n: int): ref RC4state
{
	state := array [256] of byte;
	for(i := 0; i < 256; i++)
		state[i] = byte i;

	j, t, index2: int = 0;
	for(i = 0; i < 256; i++) {
		t = int state[i];
		index2 = (int start[j] + int t + index2) & 255;
		state[i] = state[index2];
		state[index2] = byte t;
		if(++j >= n)
			j = 0;
	}

	return ref RC4state(state, 0, 0);
}

rc4(key: ref RC4state, a: array of byte, n: int)
{
	x := key.x;
	y := key.y;
	state := key.state;
	for(i := 0; i < n; i++) {
		x = (x+1)&255;
		tx := int state[x];
		y = (y+tx)&255;
		ty := int state[y];
		state[x] = byte ty;
		state[y] = byte tx;
		a[i] ^= state[(tx+ty)&255];
	}
	key.x = x;
	key.y = y;
}

