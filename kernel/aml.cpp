/* ***
 * Copyright (c) 2014-2021  Meisaka Yukara
 * See LICENSE for full details
 * e6af70ca
 */

#include "acpi.hpp"
#include "ktext.hpp"
#include "kio.hpp"

namespace acpi {
uint8_t acpi_checksum(const void *where, size_t length);
struct AMLName;

struct NSEntry {
	static const char* e_types[];
	char name[5];
	uint8_t e_type;
	uint32_t aml_length;
	const uint8_t *aml_source;
	NSEntry *up;
	NSEntry *inner;
	NSEntry *next;
	void init() {
		name[0] = 0;
		name[1] = 0;
		name[2] = 0;
		name[3] = 0;
		name[4] = 0;
		e_type = 0;
		aml_source = nullptr;
		up = nullptr;
		next = nullptr;
		inner = nullptr;
	}
	void display() {
		char name[5];
		name[0] = this->name[0];
		name[1] = this->name[1];
		name[2] = this->name[2];
		name[3] = this->name[3];
		name[4] = 0;
		xiv::printf("%s:%s:u=%x", name, e_types[e_type], up);
	}
};
const char* NSEntry::e_types[] = {"Scope", "Device", "Name", "Method", "4", "5"};
struct NameBlock {
	typedef NSEntry Entry;
	constexpr static unsigned ESIZE = sizeof(Entry);
	constexpr static unsigned ECOUNT = (0x1000 - 64) / ESIZE;
	static NameBlock *root_block;
	static NameBlock *curr_block;
	NameBlock *next_block;
	Entry entries[ECOUNT];
	static void init_block() {
		if(!root_block) {
			curr_block = root_block = (NameBlock*)mem::vmm_request(sizeof(NameBlock), nullptr, 0, mem::RQ_ALLOC | mem::RQ_RW);
			root_block->init();
			auto &e = root_block->entries[0];
			e.init();
			e.name[0] = '\\';
			return;
		}
		NameBlock *new_block = (NameBlock*)mem::vmm_request(sizeof(NameBlock), nullptr, 0, mem::RQ_ALLOC | mem::RQ_RW);
		new_block->init();
		curr_block->next_block = new_block;
		curr_block = new_block;
	}
	void init() {
		for(unsigned i = 0; i < ECOUNT; i++) {
			entries[i].init();
		}
		next_block = nullptr;
	}
	static Entry* get_root() {
		return &root_block->entries[0];
	}
	static Entry* find(const AMLName &name, Entry *ctx = nullptr);
	static Entry* find_or_new(AMLName &name, Entry *ctx = nullptr);
	static Entry* make(const AMLName &name);
};
NameBlock* NameBlock::root_block = nullptr;
NameBlock* NameBlock::curr_block = nullptr;

struct AMLName {
	char name[5];
	bool success;
	bool is_root;
	bool ascend;
	AMLName *upper;
	AMLName *lower;
	NameBlock::Entry *context;
	operator bool() const { return success; }
};
static const AMLName invalid_name{{}, false, false, false, nullptr, nullptr, nullptr};

NameBlock::Entry* NameBlock::find(const AMLName &name, Entry *ctx) {
	Entry *s = nullptr;
	if(name.name[0] == 0) {
		return nullptr;
	}
	if(ctx) {
		s = ctx->inner;
	}
	while(s) {
		if(s->name[0] == name.name[0]
			&& s->name[1] == name.name[1]
			&& s->name[2] == name.name[2]
			&& s->name[3] == name.name[3]) {
			return s;
		}
		s = s->next;
	}
	return nullptr;
}
NameBlock::Entry* NameBlock::find_or_new(AMLName &name, Entry *ctx) {
	Entry *root = get_root();
	if(ctx) {
		root = ctx;
	}
	Entry **s_in = &root->inner;
	Entry *s = *s_in;
	if(name.name[0] == 0) {
		name.name[0] = '_';
		name.name[1] = '_';
		name.name[2] = '_';
		name.name[3] = '_';
	}
	while(s) {
		if(s->name[0] == name.name[0]
			&& s->name[1] == name.name[1]
			&& s->name[2] == name.name[2]
			&& s->name[3] == name.name[3]) {
			return s;
		}
		s_in = &s->next;
		s = *s_in;
	}
	//xiv::printattr(1);
	//xiv::printf(" making-name:%s", name.name);
	//xiv::printattr(0);
	s = make(name); // didn't find name
	s->up = root;
	*s_in = s;
	return s;
}
NameBlock::Entry* NameBlock::make(const AMLName &name) {
	while(true) {
		for(unsigned i = 0; i < ECOUNT; i++) {
			if(curr_block->entries[i].name[0] == 0) {
				Entry &e = curr_block->entries[i];
				e.init();
				e.name[0] = name.name[0];
				e.name[1] = name.name[1];
				e.name[2] = name.name[2];
				e.name[3] = name.name[3];
				return &e;
			}
		}
		init_block();
	}
	return nullptr;
}

struct AMLData {
	uint32_t value;
	uint32_t value_hi;
	const uint8_t *start;
	const uint8_t *end;
	uint8_t d_type;
};

struct AMLReader {
	typedef const uint8_t *Ptr;
	using Name = AMLName;
	Ptr current;
	Ptr end;
	int depth;
	NameBlock::Entry *scope;
	NameBlock::Entry *named_scope;
	static constexpr unsigned NAME_PATH_LIMIT = 16;
	Name name_path[NAME_PATH_LIMIT];
	struct Save {
		Ptr current;
		NameBlock::Entry *scope;
	};
	AMLReader() : current{nullptr}, end{nullptr}, scope{nullptr}, depth{0} {}
	AMLReader(const uint8_t *c, const uint8_t *e) : current{c}, end{e},
	   scope{nullptr}, depth{0} {}
	Save save() {
		return {current, scope};
	}
	void cancel(Save &v) {
		current = v.current;
		scope = v.scope;
	}
	Ptr offset(int pkg_length) {
		return current + pkg_length;
	}
	void enter_scope(NameBlock::Entry *s) {
		//xiv::printattr(1);
		//xiv::printf("enter_scope:%x", s);
		//xiv::printattr(0);
		scope = s;
		depth++;
	}
	void leave_scope(Save &v) {
		cancel(v);
		//xiv::printattr(1);
		//xiv::printf("leave_scope:%x", scope);
		//xiv::printattr(0);
		depth--;
	}
	int get() {
		int v = -1;
		if(current < end) {
			v = *(this->current++);
			//xiv::printf(" get('%c')", v);
		}
		return v;
	}
	void back() {
		current--;
	}

	Name name_seg() {
		char name[5];
		name[4] = 0;
		int c;
		Save s = save();
		if(-1 == (c = this->get())) return invalid_name;
		if((c >= 'A' && c <= 'Z') || c == '_') {
			name[0] = c;
		} else {
			cancel(s);
			return {{}, false};
		}
		if(-1 == (c = this->get())) return invalid_name;
		name[1] = c;
		if(-1 == (c = this->get())) return invalid_name;
		name[2] = c;
		if(-1 == (c = this->get())) return invalid_name;
		name[3] = c;
		//xiv::printf("\"%s\"", name);
		return {{name[0], name[1], name[2], name[3], 0}, true, false, true, nullptr};
	}

	Name name_string() {
		Save s = save();
		int first = get(); if(first == -1) return invalid_name;
		//xiv::printf(" NameString: '%c'", first);
		auto *context = scope;
		bool is_root = false;
		Name *upper = nullptr;
		if(first == '^') { // prefix
			while(first == '^') {
				xiv::printf(" '%c'", first);
				s = save();
				first = get(); if(first == -1) return invalid_name;
			}
		} else if(first == '\\') { // root
			//xiv::printf(" Root '%c'", first);
			context = NameBlock::get_root();
			is_root = true;
			s = save();
			first = get(); if(first == -1) return invalid_name;
		}
		// name path
		if(first == 0) { // null name
			xiv::printf(" NullName TODO");
		} else if(first == 0x2e) { // dual name
			auto name = name_seg();
			if(!name) return invalid_name;
			xiv::printf(" DualName TODO");
			if(!name_seg()) return invalid_name;
		} else if(first == 0x2f) { // multi name
			//xiv::printf(" MultiName");
			first = get(); if(first == -1) return invalid_name;
			uint8_t segcount = (first & 0xff);
			AMLName name = invalid_name;
			uint32_t multi_index = 0;
			while(segcount--) {
				name = name_seg();
				if(!name) return invalid_name;
				name.is_root = is_root; is_root = false;
				name.lower = nullptr;
				name.upper = nullptr;
				context = NameBlock::find(name, context);
				name.context = context;
				if(multi_index < NAME_PATH_LIMIT) {
					name.upper = upper;
					name_path[multi_index] = name;
					upper = name_path + multi_index;
					if(name.upper) name.upper->lower = upper;
					multi_index++;
				}
			}
			named_scope = context;
			return name;
		} else {
			cancel(s);
			auto name = name_seg();
			if(!name) return invalid_name;
			name.is_root = is_root; is_root = false;
			name.lower = nullptr;
			name.upper = nullptr;
			context = NameBlock::find(name, context);
			name.context = context;
			named_scope = context;
			return name;
		}
		return invalid_name;
	}
	void declare_name(Name &name) {
		using namespace xiv;
		Name *top = nullptr;
		NSEntry *context = scope;
		for(auto *u = &name; u; u = u->upper) {
			top = u;
			if(u->upper && u->upper->lower != u) {
				u->upper->lower = u;
			}
		}
		NSEntry *root_context = NameBlock::get_root();
		if(top->is_root) context = root_context;
		for(auto *u = top; u; u = u->lower) {
			if(!u->context) {
				if(context == root_context) {
					printf("\\.");
				}
				//printf("cx:%x:", context);
				u->context = context = NameBlock::find_or_new(*u, context);
			} else {
				context = u->context;
			}
			printf("%s.", u->name);
		}
	}
	int pkg_length() {
		// lead byte:
		//  bits (7-6) = count of following bytes
		//   0 count means:
		//  bits 0-5 == length
		//   n count means:
		//  bits 0-3 == low nybble of length
		//   n LE ordered bytes follow
		int c = this->get(); if(c == -1) return -1;
		//xiv::printf("pkglen[0]=%x,", c);
		int n = c >> 6;
		if(n == 0) return c & 0x3f;
		int len = c & 0xf;
		c = this->get(); if(c == -1) return -1;
		len |= (c << 4);
		//xiv::printf("pkglen[1]=%x:%x,", c, len);
		if(n > 1) {
			c = this->get(); if(c == -1) return -1;
			//xiv::printf("pkglen[2]=%x,", c);
			len |= (c << 12);
		}
		if(n > 2) {
			c = this->get(); if(c == -1) return -1;
			//xiv::printf("pkglen[3]=%x,", c);
			len |= (c << 20);
		}
		return len;
	}
	bool localorarg_obj() {
		using namespace xiv;
		int c = this->get(); if(c == -1) return false;
		switch(c & 0xff) {
		// LocalTerm
		case 0x60: printf(" Local0"); break;
		case 0x61: printf(" Local1"); break;
		case 0x62: printf(" Local2"); break;
		case 0x63: printf(" Local3"); break;
		case 0x64: printf(" Local4"); break;
		case 0x65: printf(" Local5"); break;
		case 0x66: printf(" Local6"); break;
		case 0x67: printf(" Local7"); break;
		// ArgTerm
		case 0x68: printf(" Arg0"); break;
		case 0x69: printf(" Arg1"); break;
		case 0x6a: printf(" Arg2"); break;
		case 0x6b: printf(" Arg3"); break;
		case 0x6c: printf(" Arg4"); break;
		case 0x6d: printf(" Arg5"); break;
		case 0x6e: printf(" Arg6"); break;
		default:
			return false;
		}
		return true;
	}
	bool debug_object() {
		using namespace xiv;
		int c = this->get(); if(c == -1) return false;
		if(c != 0x5b) return false;
		c = get(); if(c == -1) return false;
		if(c != 0x31) return false; // DebugOp
		printf(" DebugObject");
		return true;
	}
	bool super_name() {
		using namespace xiv;
		Save s = save();
		if(localorarg_obj()) return true;
		cancel(s);
		if(name_string()) return true;
		cancel(s);
		if(type_six_opcode()) return true;
		cancel(s);
		if(debug_object()) return true;
		cancel(s);
		return false;
	}
	bool target() {
		using namespace xiv;
		Save s = save();
		int c = get(); if(c == -1) return false;
		if(c == 0) {
			printf(" NullName");
			return true;
		}
		cancel(s);
		return super_name();
	}
	bool ext_op() {
		using namespace xiv;
		int c = get(); if(c == -1) return false;
		switch(c & 0xff) {
		case 0: printf(" ExtOp-00"); break;
		case 0x02:
			printf(" EventOp"); break;
		case 0x12:
			printf(" CondRefOf");
			return super_name()
				&& target();
		case 0x13:
			printf(" CreateFieldOp"); break;
		case 0x1f:
			printf(" LoadTableOp"); break;
		case 0x23:
			printf(" AcquireOp"); break;
		case 0x25:
			printf(" WaitOp"); break;
		case 0x28:
			printf(" FromBCDOp"); break;
		case 0x29:
			printf(" ToBCDOp"); break;
		case 0x2a:
			printf(" ExtOpReserved"); break;
		case 0x30:
			printf(" RevisionOp"); break;
		case 0x31:
			printf(" DebugOp"); break;
		case 0x33:
			printf(" TimerOp"); break;

		case 0x86:
			printf(" IndexFieldOp"); break;
		case 0x87:
			printf(" BankFieldOp"); break;
		case 0x88:
			printf(" DataRegionOp"); break;
		default:
			printf(" not-extop: %x", c);
			return false;
		}
		return true;
	}
	bool type_six_opcode() {
		using namespace xiv;
		int c = this->get(); if(c == -1) return false;
		switch(c & 0xff) {
		case 0x71:
			printf(" RefOfOp");
			return super_name();
		case 0x83:
			printf(" DerefOf");
			return term_arg(); // expect ObjectReference | String
		case 0x88:
			printf(" Index");
			return term_arg() // Buffer | Package | String
				&& term_arg() // Integer
				&& target();
		// or UserTerm which is confusing... :3
		default:
			return false;
		}
	}
	bool type_one_opcode() {
		using namespace xiv;
		Save s = save();
		Ptr p_end;
		int pkg_len;
		int c = this->get(); if(c == -1) return false;
		switch(c & 0xff) {
		case 0x5b:
			c = this->get(); if(c == -1) return false;
			switch(c & 0xff) {
			case 0x20:
				printf(" LoadOp"); return true;
			case 0x21:
				printf(" StallOp"); return true;
			case 0x22:
				printf(" SleepOp"); return true;
			case 0x24:
				printf(" SignalOp"); return true;
			case 0x26:
				printf(" ResetOp"); return true;
			case 0x27:
				printf(" ReleaseOp"); return true;
			case 0x32:
				printf(" FatalOp"); return true;
			default: break;
			}
			return false;
		case 0x86: printf(" NotifyOp"); return true;
		case 0x9f: printf(" ContinueOp"); return true;
		case 0xa0:
			p_end = current;
			pkg_len = pkg_length(); if(pkg_len == -1) return false;
			p_end += pkg_len;
			printf(" IfOp len=%x", pkg_len);
			term_arg(); // Predicate -> Integer
			while(current < p_end) {
				if(!term_arg()) return false;
			}
			return true;
		case 0xa1:
			p_end = current;
			pkg_len = pkg_length(); if(pkg_len == -1) return false;
			p_end += pkg_len;
			printf(" ElseOp len=%x", pkg_len);
			while(current < p_end) {
				if(!term_arg()) return false;
			}
			return true;
		case 0xa2:
			p_end = current;
			pkg_len = pkg_length(); if(pkg_len == -1) return false;
			p_end += pkg_len;
			printf(" WhileOp len=%x", pkg_len);
			term_arg();
			while(current < p_end) {
				if(!term_arg()) return false;
			}
			return true;
		case 0xa3: printf(" NoopOp"); return true;
		case 0xa4: printf(" ReturnOp"); return term_arg();
		case 0xa5: printf(" BreakOp"); return true;
		case 0xcc: printf(" BreakPointOp"); return true;
		default:
			printf(" Type1Opcode TODO!\n");
			break;
		}
		return false;
	}
	bool type_two_opcode() {
		using namespace xiv;
		Save s = save();
		int c = get(); if(c == -1) return false;
		switch(c & 0xff) {
		case 0x5b:
			c = get(); if(c == -1) return false;
			switch(c & 0xff) {
			case 0x12:
				printf(" CondRefOf");
				return super_name()
					&& target();
			default:
				printf(" Type2-ExtPrefix: %x", c);
				return false;
			}
		case 0x72:
			printf(" AddOp");
			return term_arg()
				&& term_arg()
				&& target();
		case 0x83:
			printf(" DerefOf");
			return term_arg(); // expect ObjectReference | String
		case 0x87: printf(" SizeOf"); return super_name();
		case 0x88:
			printf(" Index");
			return term_arg() // Buffer | Package | String
				&& term_arg() // Integer
				&& target();
		case 0x90: printf(" Land"); return term_arg() && term_arg();
		case 0x91: printf(" Lor"); return term_arg() && term_arg();
		case 0x92: printf(" Lnot"); return term_arg();
		case 0x93: printf(" LEqual"); return term_arg() && term_arg();
		case 0x94: printf(" LGreater"); return term_arg() && term_arg();
		case 0x95: printf(" LLess"); return term_arg() && term_arg();
		default:
			cancel(s);
			if(name_string()) {
				printf(" Invoke?");
				return true;
			}
			printf(" not-impl: TwoOp: %x\n", c);
			return false;
		}
		return true;
	}
	bool term_arg() {
		using namespace xiv;
		// Type2Opcode | DataObject | ArgObj | LocalObj
		Save s = save();
		if(localorarg_obj()) return true;
		cancel(s);
		if(data_object()) return true;
		cancel(s);
		if(type_two_opcode()) return true;
		cancel(s);
		return false;
	}
	bool get_byte(AMLData &out, bool show) {
		out.start = current;
		int c = get(); if(c == -1) return false;
		out.value = (c & 0xff);
		out.end = current;
		out.d_type = 0xa;
		if(show) xiv::printf(" Byte=%x", out.value);
		return true;
	}
	bool get_word(AMLData &out, bool show) {
		uint32_t data;
		out.start = current;
		int c = get(); if(c == -1) return false;
		data = (c & 0xff);
		c = get(); if(c == -1) return false;
		data |= (c & 0xff) << 8;
		out.value = data;
		out.end = current;
		out.d_type = 0xb;
		if(show) xiv::printf(" Word=%x", data);
		return true;
	}
	bool get_dword(AMLData &out, bool show) {
		uint32_t data;
		out.start = current;
		int c = get(); if(c == -1) return false;
		data = (c & 0xff);
		c = get(); if(c == -1) return false;
		data |= (c & 0xff) << 8;
		c = get(); if(c == -1) return false;
		data |= (c & 0xff) << 16;
		c = get(); if(c == -1) return false;
		data |= (c & 0xff) << 24;
		out.value = data;
		out.end = current;
		out.d_type = 0xc;
		if(show) xiv::printf(" DWord=%x", data);
		return true;
	}
	bool get_qword(AMLData &out, bool show) {
		uint32_t data;
		out.start = current;
		int c = get(); if(c == -1) return false;
		data = (c & 0xff);
		c = get(); if(c == -1) return false;
		data |= (c & 0xff) << 8;
		c = get(); if(c == -1) return false;
		data |= (c & 0xff) << 16;
		c = get(); if(c == -1) return false;
		data |= (c & 0xff) << 24;
		out.value = data;
		c = get(); if(c == -1) return false;
		data = (c & 0xff);
		c = get(); if(c == -1) return false;
		data |= (c & 0xff) << 8;
		c = get(); if(c == -1) return false;
		data |= (c & 0xff) << 16;
		c = get(); if(c == -1) return false;
		data |= (c & 0xff) << 24;
		out.value_hi = data;
		out.end = current;
		out.d_type = 0xe;
		uint64_t ldata = (out.value) | ((uint64_t)out.value_hi << 32);
		if(show) xiv::printf(" QWord=%lx", ldata);
		return true;
	}
	bool computational_data(AMLData &data, bool show) {
		using namespace xiv;
		int c = this->get(); if(c == -1) return false;
		switch(c & 0xff) {
		case 0xa: return get_byte(data, show);
		case 0xb: return get_word(data, show);
		case 0xc: return get_dword(data, show);
		case 0xd:
		{
			Save start = save();
			c = get(); if(c == -1) return false;
			while(c != 0) {
				c = get(); if(c == -1) return false;
			}
			printf(" String \"%s\"", start);
			return true;
		}
		case 0xe: return get_qword(data, show);
		// ConstObj
		case 0:
			printf(" ZeroOp");
			data.value = 0;
			data.value_hi = 0;
			data.start = current;
			data.end = current;
			data.d_type = 1;
			return true;
		case 1:
			printf(" OneOp");
			data.value = 1;
			data.value_hi = 0;
			data.start = current;
			data.end = current;
			data.d_type = 1;
			return true;
		case 0xff:
			printf(" OnesOp");
			data.value = 0xffffffff;
			data.value_hi = 0xffffffff;
			data.start = current;
			data.end = current;
			data.d_type = 1;
			return true;
		case 0x5b: // RevisionOp ? -> 0x5b 0x30
		{
			//Save s = save();
			c = this->get(); if(c == -1) return false;
			if(c == 0x30) {
				printf(" not-impl: RevisionOp");
			}
			return false;
		}
		case 0x11: // DefBuffer ? -> 0x11 PkgLength BufferSize ByteList
		{
			Ptr p_end = current;
			int pkg_len = this->pkg_length(); if(pkg_len == -1) return false;
			p_end += pkg_len;
			if(show) printf(" DefBuffer:len=");
			if(!term_arg()) return false; // BufferSize -> Integer
			if(show) printf(" data[");
			data.start = current;
			data.end = p_end;
			data.d_type = 0x11;
			if(show) while(current < p_end) {
				int b = get(); if(b == -1) return false;
				printf(" %02x", b);
			}
			if(show) printf("]");
			current = p_end; // skip the contents if not shown
			return true;
		}
		default:
			return false;
		}
		return true;
	}
	bool data_object() {
		using namespace xiv;
		Save s = save();
		AMLData data;
		if(computational_data(data, false)) return true;
		cancel(s);
		int c = this->get(); if(c == -1) return false;
		if(c == 0x12) {
			Ptr p_end = current;
			int pkg_len = pkg_length(); if(pkg_len == -1) return false;
			p_end += pkg_len;
			printf(" DefPackage pkglen=%x", pkg_len);
			uint8_t elem;
			c = get(); if(c == -1) return false;
			elem = (c & 0xff);
			while(current < p_end) {
				Save s = save();
				if(!name_string()) {
					cancel(s);
					if(!data_ref_object(data)) return false;
				}
			}
			return true;
		} else if(c == 0x13) {
			printf(" not-impl: data_object: DefVarPackage\n");
		}
		this->cancel(s);
		return false;
	}
	bool data_ref_object(AMLData &data, bool show = false) {
		using namespace xiv;
		Save s = save();
		if(computational_data(data, show)) return true;
		cancel(s);
		if(term_arg()) {
			data.d_type = 0; // TODO evaluator for terms?
			return true;
		}
		cancel(s);
		printf(" unknown data_ref_object\n");
		return false;
	}
	bool object() {
		// NameSpaceModifierObj | NamedObj
		//  NameSpaceModifierObj = DefAlias | DefName | DefScope
		//  NamedObj = DefBankField | DefCreateBitField | DefCreate...
		//   | DefDataRegion | DefExternal | DefOpRegion | DefPowerRes
		//   | DefProcessor | DefThermalZone
		using namespace xiv;
		Save s = save();
		Ptr p_end;
		AMLData data;
		int c = get(); if(c == -1) return false;
		int pkg_len;
		switch(c & 0xff) {
		case 0x6: printf(" AliasOp"); break;
		case 0x8:
		{
			auto name = name_string();
			if(!name) return false;
			printf("Name=>%s", name.name);
			Save v = save();
			if(!data_ref_object(data)) return false;
			declare_name(name);
			name.context->e_type = 2;
			name.context->aml_source = v.current;
			name.context->aml_length = current - v.current;
			//NameBlock::find_or_new(name, this->scope);
			return true;
		}
		case 0x10:
		{
			p_end = current;
			pkg_len = pkg_length(); if(pkg_len == -1) return false;
			p_end += pkg_len;
			auto s = save();
			auto name = name_string();
			if(!name) return false;
			printf(" Scope(");
			declare_name(name);
			printf(")");
			enter_scope(name.context);
			while(current < p_end) {
				if(!term_obj()) return false;
			}
			leave_scope(s);
			current = p_end;
			return true;
		}
		case 0x14: // method op
		{
			// pkg length
			// name_string()
			// method flags = byte
			//  (0 - 2) = arg count
			//  (3) = serialize flag, 1 == true
			//  (4 - 7) sync level 0x0 - 0xf
			// termlist
			p_end = current;
			pkg_len = pkg_length(); if(pkg_len == -1) return false;
			p_end += pkg_len;
			auto s = save();
			auto name = name_string();
			if(!name) return false;
			printf(" Method(");
			declare_name(name);
			name.context->e_type = 3;
			c = get(); if(c == -1) return false;
			int arg_count = c & 0x7;
			uint8_t serialize = (c & 8) ? 1 : 0;
			int sync_level = c >> 4;
			printf(",a=%x%s,s=%x)", arg_count,
				serialize ? ",ser" : "", sync_level);
			enter_scope(name.context);
			current = p_end; // XXX
			while(current < p_end) {
				if(!term_obj()) return false;
			}
			leave_scope(s);
			current = p_end;
			return true;
		}
		case 0x5b: // ExtOp
			c = get(); if(c == -1) return false;
			switch(c & 0xff) {
			case 0x80:
				printf(" OpRegionOp");
				if(!name_string()) return false;
				c = get(); if(c == -1) return false;
				// 0 System Memory
				// 1 System IO
				// 2 PCI Config
				// 3 EmbeddedControl
				// 4 SMBus
				// 5 System CMOS
				// 6 PCIBar Target
				// 7 IPMI
				// 8 GPIO
				// 9 SerialBus
				// 0xA PCC
				// 0x80-0xff -> OEM
				printf(" RgnSpace=%x", c);
				return term_arg() // RegionOffset -> Integer
					&& term_arg(); // RegionLen -> Integer
			case 0x81:
			{
				Ptr p_end = current;
				pkg_len = pkg_length(); if(pkg_len == -1) return false;
				p_end += pkg_len;
				printf(" FieldOp pkglen=%x [%x]", pkg_len, *p_end);
				if(!name_string()) return false;
				c = get(); if(c == -1) return false; // FieldFlags
				// bit 0-3: AccessType
				//  0 Any
				//  1 Byte
				//  2 Word
				//  3 DWord
				//  4 QWord
				//  5 Buffer
				//  6-15 Reserved
				// bit 4 lock rule: 1 == Lock else nolock
				// bit 5-6: update rule
				//  0 preserve
				//  1 write as ones
				//  2 write as zeros
				// bit 7 must be zero
				printf(" FieldFlags=%x,L=%c,U=%s",
					c & 0xf, (c & 0x10) ? 'Y' : 'N',
					(c & 0x60) ? (((c&0x60)==0x40) ? "WrOnes" : "WrZeros"): "Preserve");
				while(current < p_end) {
					Save s = save();
					// Field Elements
					// NamedField | ReservedField | AccessField |
					// ExtendedAccessField | ConnectField
					// NamedField = NameSeg PkgLength
					c = get(); if(c == -1) return false;
					if(c == 0) {
						// ReservedField = 0x00 PkgLength
						pkg_len = pkg_length(); if(pkg_len == -1) return false;
						printf(" ReservedField pkglen=%x", pkg_len);
					} else if(c == 1) {
						// AccessField = 0x01 AccessType AccessAttrib
						c = get(); if(c == -1) return false;
						uint8_t access_type = (c & 0xff);
						c = get(); if(c == -1) return false;
						uint8_t access_attrib = (c & 0xff);
						printf(" AccessField type=%x attrib=%x", access_type, access_attrib);
					} else if(c == 2) {
						// ConnectField = 0x02 NameString | BufferData
						printf(" ConnectField TODO");
					} else if(c == 3) {
						// ExtendedAccessField = 0x03 AccessType ExtendedAccessAttrib AccessLength
						//  ExtendedAccessAttrib = Byte:
						//   0x0b = AttribBytes
						//   0x0e = AttribRawBytes
						//   0x0f = AttribRawProcess
						//  AccessLength = ByteConst maybe? spec doesn't say.... :3
						printf(" ExtendedAccessField TODO");
					} else {
						printf(" NamedField");
						cancel(s);
						if(!name_seg()) return false;
						pkg_len = pkg_length(); if(pkg_len == -1) return false;
						printf(",l=%x", pkg_len);
					}
					if(current >= p_end) printf(" EOP");
				}
				return true;
			}
			case 0x82: // Device
			{
				p_end = current;
				pkg_len = pkg_length(); if(pkg_len == -1) return false;
				p_end += pkg_len;
				printf(" Device");
				auto s = save();
				auto name = name_string();
				if(!name) {
					cancel(s);
					return false;
				}
				printf("(\"%s\")", name.name);
				if(depth > 2) {
					//current = p_end; return true; // XXX debugging trim
				}
				declare_name(name);
				//current = p_end; // TODO, parse these
				name.context->e_type = 1;
				enter_scope(name.context);
				//printf("\nDSDT-Device:   ");
				while(current < p_end) {
					if(!term_obj()) return false;
				}
				leave_scope(s);
				current = p_end;
				return true;
			}
			case 0x83: // Processor
			{
				p_end = current;
				pkg_len = pkg_length(); if(pkg_len == -1) return false;
				p_end += pkg_len;
				printf(" Processor pkglen=%x", pkg_len);
				if(!name_string()) return false;
				c = get(); if(c == -1) return false;
				printf(",id=%x,pblk=", c);
				AMLData pblockaddr;
				if(!get_dword(pblockaddr, true)) return false;
				c = get(); if(c == -1) return false;
				printf(",pblklen=%x", c);
				current = p_end; // TODO skip terms
				while(current < p_end) {
					if(!term_obj()) return false;
				}
				return true;
			}
			case 0x84:
				printf(" PowerResOp"); break;
			case 0x85:
				printf(" ThermalZoneOp"); break;
			case 0x01:
				printf(" MutexOp");
				name_string();
				c = get(); if(c == -1) return false;
				printf(" sync=%x", c & 0xf);
				return true;
			default:
				break;
			}
			return false;
		default: break;
		}
		return false;
	}
	bool term_obj() {
		using namespace xiv;
		Save s = save();
		// Object | Type1Opcode | Type2Opcode
		if(object()) return true;
		cancel(s);
		if(type_two_opcode()) return true;
		cancel(s);
		if(type_one_opcode()) return true;
		cancel(s);
		return false;
	}
};

void display_namespace_inner(int l, NameBlock::Entry *co) {
	using namespace xiv;
	if(l > 8) {
		printf("%s -> depth!\n", co->name);
		return;
	}
	auto *c = co->inner;
	while(c) {
		do {
			//if(c->e_type != 1) break;
			if(c->name[0] == 'P' && c->name[1] == 'E') break;
			if(c->name[0] == 'S' && c->name[1] == '1' && c->name[2] == 'F') break;
			if(c->name[0] == '_' && c->name[1] == 'H' && c->name[2] == 'I') break;
			if(c->name[0] == '_' && c->name[1] == 'U' && c->name[2] == 'I') break;
			if(c->name[0] == '_' && c->name[1] == 'S' && c->name[2] == 'T') break;
			for(int k = l; k-->0;) printf("   ");
			c->display();
			printf("%x\n", c);
		} while(false);
		display_namespace_inner(l + 1, c);
		c = c->next;
	}
}

void display_namespace() {
	using namespace xiv;
	auto *root = NameBlock::get_root();
	auto *c = root->inner;
	printf("\nnamespace:\n");
	while(c) {
		printf("\\");
		c->display();
		printf("%x\n", c);
		display_namespace_inner(1, c);
		c = c->next;
	}
}

AMLReader debug_aml;

void debug_namespace() {
	using namespace xiv;
	auto *item = debug_aml.scope;
	printf("name:%s\n", item->name);
	auto *c = item->inner;
	AMLData data;
	while(c) {
		c->display();
		printf(",na=%x %s ", c, (c->inner != nullptr) ? "*" : "");
		if(c->e_type == 2) { // Name -> data ref object
			debug_aml.current = c->aml_source;
			debug_aml.end = c->aml_source + c->aml_length;
			debug_aml.data_ref_object(data);
			if(data.d_type >= 0xa && data.d_type <= 0xc) {
				printf("value=%x", data.value);
			} else if(data.d_type == 0xe) {
				uint64_t lvalue = data.value | ((uint64_t)data.value_hi << 32);
				printf("value=%lx", data.value);
			}
			if(c->name[0] == '_'
				&& c->name[1] == 'C'
				&& c->name[2] == 'R'
				&& c->name[3] == 'S'
				&& data.d_type == 0x11) {
				debug_aml.current = data.start;
				debug_aml.end = data.end;
				printf("CRS:len=%x:", data.end - data.start);
				while(true) {
					int b = debug_aml.get();
					if(b == -1) {
						printf(" EndCRS");
						break;
					}
					int iname = (b >> 3);
					if(b & 0x80) {
						printf(" large item tag! %x", b);
						break; // tag not zero! large data
					}
					int len = b & 7;
					auto *end = debug_aml.current + len;
					int n = 0;
					switch(iname) {
					case 4: // IRQ format
						if(len != 2 && len != 3) break;
						printf("IRQ:[");
						b = debug_aml.get(); if(b == -1) break;
						for(n = 0; n < 8; n++) {
							if(b & (1 << n)) {
								printf(" IRQ%d", n);
							}
						}
						b = debug_aml.get(); if(b == -1) break;
						for(; n < 16; n++) {
							if(b & (1 << (n - 8))) {
								printf(" IRQ%d", n);
							}
						}
						if(len == 3) {
							b = debug_aml.get(); if(b == -1) break;
							printf(" %s Act%s %s%s",
								(b & 1) ? "Edge" : "Level",
								(b & (1<<3)) ? "Low" : "High",
								(b & (1<<4)) ? "Shared" : "Exclusive",
								(b & (1<<5)) ? "WakeCap" : "");
						}
						printf("]");
						break;
					case 8: // I/O port
					{
						if(len != 7) break;
						b = debug_aml.get(); if(b == -1) break;
						printf("I/O port:[decode=%s", (b == 1) ? "16b" : "10b");
						uint16_t min_base, max_base;
						uint8_t align, length;
						b = debug_aml.get(); if(b == -1) break;
						min_base = b & 0xff;
						b = debug_aml.get(); if(b == -1) break;
						min_base |= (b & 0xff) << 8;
						b = debug_aml.get(); if(b == -1) break;
						max_base = b & 0xff;
						b = debug_aml.get(); if(b == -1) break;
						max_base |= (b & 0xff) << 8;
						b = debug_aml.get(); if(b == -1) break;
						align = b & 0xff;
						b = debug_aml.get(); if(b == -1) break;
						length = b & 0xff;
						printf(",min=%x,max=%x,align=%x,len=%x]",
							min_base, max_base, align, length);
						break;
					}
					case 5: // DMA format
					case 6: // start dependant functions
					case 7: // end dependant functions
					case 9: // fixed location I/O port
					case 0xa: // fixed DMA
					case 0xe: // vendor defined
					case 0xf: // end tag
					default: // reserved
						printf("can not decode: %x", b);
						break;
					}
					debug_aml.current = end;
				}
			}
		}
		printf("\n");
		c = c->next;
	}
}

void debug_acpi(const char *arg, int len) {
	using namespace xiv;
	debug_aml.current = (const uint8_t*)arg;
	debug_aml.end = debug_aml.current + len;
	int c = debug_aml.get();
	if(c == -1) {
		display_namespace();
		return;
	}
	switch(c) {
	case 'e': // enter
	{
		AMLName name = debug_aml.name_string();
		if(!name) {
			printf("invalid name\n");
			return;
		}
		printf("name:%s, cx:%x\n", name.name, name.context);
		if(name.context != nullptr) {
			debug_aml.enter_scope(name.context);
			debug_namespace();
		}
		break;
	}
	case '.': // up
	case 'u':
		if(debug_aml.scope->up) {
			debug_aml.enter_scope(debug_aml.scope->up);
			debug_namespace();
		}
		break;
	case 'l': // list
		debug_namespace();
		break;
	case 'r': // root
		debug_aml.enter_scope(NameBlock::get_root());
		break;
	}
}

void load_dsdt(const ACPITableHeader *dsdt_base, uint64_t dsdt_phy) {
	using namespace xiv;
	if(dsdt_base->sig_v != 0x54445344) {
		printf("ACPI-DSDT: bad sig %x\n", dsdt_base->sig_v);
		return;
	}
	const uint8_t *dsdt_data = (const uint8_t *)(dsdt_base + 1);
	const uint8_t *dsdt_end = dsdt_data + (dsdt_base->length - sizeof(ACPITableHeader));
	if(((uintptr_t)dsdt_data & ~0xfff) != ((uintptr_t)dsdt_end & ~0xfff)) {
		uint32_t npages = ((uintptr_t)dsdt_end & ~0xfff) - ((uintptr_t)dsdt_data & ~0xfff);
		if((uintptr_t)dsdt_end & 0xfff) npages += 0x1000;
		printf("ACPI-DSDT: length: %x %x\n", dsdt_base->length, npages);
		const uint8_t *v_page = (const uint8_t*)mem::vmm_request(npages, nullptr, dsdt_phy & ~0xfff, 0);
		dsdt_base = (const ACPITableHeader *)(v_page + (dsdt_phy & 0xfff));
		dsdt_data = (const uint8_t *)(dsdt_base + 1);
		dsdt_end = dsdt_data + (dsdt_base->length - sizeof(ACPITableHeader));
	}
	if(acpi_checksum(dsdt_base, dsdt_base->length)) {
		printf("ACPI-DSDT: bad checksum\n");
		return;
	}
	printf("ACPI-DSDT: %x\n", dsdt_base->sig_v);
	NameBlock::init_block();
	AMLReader aml = AMLReader(dsdt_data, dsdt_end);
	aml.enter_scope(NameBlock::get_root());
	debug_aml.enter_scope(NameBlock::get_root());
	uint32_t i = 0;
	int pkg_len;
	for(;; i++) {
		printf("DSDT: ");
		AMLReader::Save s = aml.save();
		if(aml.term_obj()) {
			printf("\n");
			continue;
		}
		aml.cancel(s);
		int c = aml.get(); if(c == -1) break;
		switch(c & 0xff) {
		case 0x11: printf(" BufferOp"); break;
		case 0x12: printf(" PackageOp"); break;
		case 0x13: printf(" VarPackageOp"); break;
		case 0x15: printf(" ExternalOp"); break;
		case 0x2e: printf(" '.'"); break;
		case 0x2f: printf(" '/'"); break;
		case 0x30:
		case 0x31:
		case 0x32:
		case 0x33:
		case 0x34:
		case 0x35:
		case 0x36:
		case 0x37:
		case 0x38:
		case 0x39:
			printf(" '%c'", c); break;
		case 0x5b:
			printf(" ExtOpPrefix");
			break;
		case 0x5c:
			printf(" '\\'"); break;
		case 0x72:
			printf(" AddOp");
			aml.term_arg();
			aml.term_arg();
			aml.target();
			break;
		case 0x73: printf(" ConcatOp"); break;
		case 0x74: printf(" SubtractOp"); break;
		case 0x75:
			printf(" IncrementOp");
			aml.super_name();
			break;
		case 0x76:
			printf(" DecrementOp");
			aml.super_name();
			break;
		case 0x77: printf(" MultiplyOp"); break;
		case 0x78: printf(" DivideOp"); break;
		case 0x79: printf(" ShiftLeftOp"); break;
		case 0x7a: printf(" ShiftRightOp"); break;
		case 0x7b: printf(" AndOp"); break;
		case 0x7c: printf(" NandOp"); break;
		case 0x7d: printf(" OrOp"); break;
		case 0x7e: printf(" NorOp"); break;
		case 0x7f: printf(" XorOp"); break;
		case 0x80: printf(" NotOp"); break;
		case 0x81: printf(" FindSetLeftBitOp"); break;
		case 0x82: printf(" FindSetRightBitOp"); break;
		case 0x83: printf(" DerefOfOp"); break;
		case 0x84: printf(" ConcatResOp"); break;
		case 0x85: printf(" ModOp"); break;
		case 0x88: printf(" IndexOp"); break;
		case 0x89: printf(" MatchOp"); break;
		case 0x8a: printf(" CreateDWordFieldOp");
			aml.term_arg(); aml.term_arg(); aml.name_string(); break;
		case 0x8b: printf(" CreateWordFieldOp");
			aml.term_arg(); aml.term_arg(); aml.name_string(); break;
		case 0x8c: printf(" CreateByteFieldOp");
			aml.term_arg(); aml.term_arg(); aml.name_string(); break;
		case 0x8d: printf(" CreateBitFieldOp");
			aml.term_arg(); aml.term_arg(); aml.name_string(); break;
		case 0x8e: printf(" ObjectTypeOp"); aml.super_name(); break;
		case 0x8f: printf(" CreateQWordFieldOp");
			aml.term_arg(); aml.term_arg(); aml.name_string(); break;
		case 0x90: printf(" LandOp"); aml.term_arg(); aml.term_arg(); break;
		case 0x91: printf(" LorOp"); aml.term_arg(); aml.term_arg(); break;
		case 0x92: printf(" LnotOp"); aml.term_arg(); break;
		case 0x93: printf(" LEqualOp"); aml.term_arg(); aml.term_arg(); break;
		case 0x94: printf(" LGreaterOp"); aml.term_arg(); aml.term_arg(); break;
		case 0x95: printf(" LLessOp"); aml.term_arg(); aml.term_arg(); break;
		case 0x96: printf(" ToBufferOp"); aml.term_arg(); break;
		case 0x97: printf(" ToDecimalStringOp"); aml.term_arg(); break;
		case 0x98: printf(" ToHexStringOp"); aml.term_arg(); break;
		case 0x99: printf(" ToIntegerOp"); aml.term_arg(); break;
		case 0x9c: printf(" ToStringOp"); aml.term_arg(); aml.term_arg(); break;
		case 0x9d: printf(" CopyObjectOp"); break;
		case 0x9e: printf(" MidOp"); break;
		default:
			printf(" AML-TODO: %x '%c'", c, c);
			break;
		}
		printf("\n");
		break;
	}
	//display_namespace();
}

} // namespace acpi

