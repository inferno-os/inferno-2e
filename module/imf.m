IMF: module
{
	mifPATH: con "/dis/lib/imf_mif.dis";
	htmlPATH: con "/dis/lib/imf_html.dis";

	TOP, BOTTOM: string;

	copyright_tag,
	emdash_tag,
	space_tag,
	zero_tag,
	lt_tag,
	gt_tag:	string;
	
	extag,
	notag,
	blocktags,
	tags:	array of string;

	init: 		fn();
	specials:	fn(text: string): string;
};
