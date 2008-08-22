#include <glib.h>

guint32 gray_decode(guint8 *bits, guint32 n_bits)
{
	guint32 gray = 0, sh = 1, div, ans;
	gint32 i;

	if(n_bits > 32)
		return 0xFFFFFFFF;

	for(i = 0; i < n_bits; i ++)
		if(bits[i])
			gray |= (1 << i);
	ans = gray;

	while(TRUE) {
		div = ans >> sh;
		ans ^= div;
		if(div <= 1)
			return ans;
		sh <<= 1;
	}
}

