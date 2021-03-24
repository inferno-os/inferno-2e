
# basic Association list structure, modelled loosely on Python dictionaries.
#    This is intended to store small numbers of unique (string, string)
#       associations
AsscList: module
{
	PATH: con "/dis/mailtool/assclist.dis";

        item: adt {
		key: string;
		value: string;
	};

	Alist: adt {
		the_list, cursor: list of ref item;

                # uniqueness flag (really should use subclasses ;) )
                #   If this is false, multiple values may be associated
                #   to one key
                unique: int;  # default to non-unique

		# basic accesses (extra int is always success/failure ind).
                #   set item n -> v (reset existing item if unique)
		setitem: fn(s: self ref Alist, n, v: string);
                #   delete all items for key n
		delitem: fn(s: self ref Alist, n: string): int;
                #   get (choose) value, or return int or 0 for failure.
                getitem: fn(s: self ref Alist, n: string): (string, int);
                #   get list of items
                getitems: fn(s: self ref Alist, n: string): list of string;
		#   pair membership test
                member: fn(s: self ref Alist, n: string, v: string): int;
		#   length
		length: fn(s: self ref Alist): int;

                # misc  (no walking!)
		copy: fn(s: self ref Alist): ref Alist;
		project: fn(s: self ref Alist, X: list of string): ref Alist;

                # walking methods.
                # NOTE inserts and deletes shouldn't be done to a list currently
                # being walked.  Also aggregates are verboten for a walking list.
		#    current key
                thiskey: fn(s: self ref Alist): (string, int);
		#    current value
                thisval: fn(s: self ref Alist): (string, int);
                #    current (key, value) pair
                thispair: fn(s: self ref Alist): ((string, string), int);
                #    advance cursor to next item
                next: fn(s: self ref Alist): int;
                #    reset cursor to first item
                first: fn(s: self ref Alist); 

                # aggregate initializer etc.
		#   never use these on a list being walked.
                addpairs: fn(s: self ref Alist, d: list of (string, string));
                frompairs: fn(d: list of (string, string)): ref Alist;
		augment: fn(s: self ref Alist, other: ref Alist);
		# union returns ambiguous results for
		#  non unique incompatible pairs.
		#  Result is unique if both inputs are.
		union: fn(a: self ref Alist, b: ref Alist): ref Alist;
		intersect: fn(a: self ref Alist, b: ref Alist): ref Alist;
		diff: fn(a: self ref Alist, b: ref Alist): ref Alist;
		subset: fn(a: self ref Alist, b: ref Alist): int;
		compatible: fn(a: self ref Alist, b: ref Alist): int;
		equal: fn(a: self ref Alist, b: ref Alist): int;

		# marshalling
		marshal: fn(a: self ref Alist): array of byte;
		unmarshal: fn(b: array of byte, start: int):
			(ref Alist, string);
		};

	# utilities for marshalling.
	quotestringtobyte: fn(s: string): array of byte;
	unquotestringfrombyte: fn(b: array of byte, start: int):
		(string, int);
};

