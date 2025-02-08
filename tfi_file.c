#include <stdio.h>
#include <string.h>

#include "tfi_file.h"

/*
$00			algo
$01			feedback
per-operator:
{
$03...		mul
$04...		detune+3
$05...		tl
$06...		rs
$07...		ar
$08...		dr
$09...		sr
$0A...		rr
$0B...		sl
$0C...		ssg
}
*/

void tfi_file_init(struct tfi_file *f) {
	memset(f, 0, sizeof(*f));
}

int tfi_file_load(struct tfi_file *tfi, uint8_t *data, size_t data_len) {
	if(data_len != 42) return -1;

	uint8_t *p = data;

	tfi->alg = *p++;
	tfi->fb = *p++;

	for(int i = 0; i < 4; i++) {
		tfi->operators[i].mul = *p++;
		tfi->operators[i].dt = *p++;
		tfi->operators[i].tl = *p++;
		tfi->operators[i].rs = *p++;
		tfi->operators[i].ar = *p++;
		tfi->operators[i].dr = *p++;
		tfi->operators[i].sr = *p++;
		tfi->operators[i].rr = *p++;
		tfi->operators[i].sl = *p++;
		tfi->operators[i].ssg_eg = *p++;
	}

	return 0;
}

int tfi_file_save(struct tfi_file *f, int (*write_fn)(void *, size_t, void *), void *data_ptr) {
	uint8_t buf[42] = { 0 };
	int bc = 0;
	buf[bc++] = f->alg;
	buf[bc++] = f->fb;
	for(int i = 0; i < 4; i++) {
		buf[bc++] = f->operators[i].mul;
		buf[bc++] = f->operators[i].dt;
		buf[bc++] = f->operators[i].tl;
		buf[bc++] = f->operators[i].rs;
		buf[bc++] = f->operators[i].ar;
		buf[bc++] = f->operators[i].dr;
		buf[bc++] = f->operators[i].sr;
		buf[bc++] = f->operators[i].rr;
		buf[bc++] = f->operators[i].sl;
		buf[bc++] = f->operators[i].ssg_eg;
	}
	return write_fn(buf, 42, data_ptr);
}

#ifdef HAVE_STDIO
void tfi_file_dump(struct tfi_file *f) {
	printf("alg=%d fb=%d\n", f->alg, f->fb);
	printf("OP MUL DT TL RS AR DR SR RR SL SSG-EG\n");
	for(int i = 0; i < 4; i++) {
		struct tfi_file_operator *op = f->operators + i;
		printf("%d  %d %d %d %d %d %d %d %d %d %d\n", i, op->mul, op->dt, op->tl, op->rs, op->ar, op->dr, op->sr, op->rr, op->sl, op->ssg_eg);
	}
}
#endif

#ifdef ENABLE_LOADERS
#include "loader.h"

static int load(void *data, int data_len, struct fm_voice_bank  *bank) {
	struct tfi_file f;
	int r = tfi_file_load(&f, data, data_len);
	if(r) return r;
	struct opn_voice *voice = fm_voice_bank_reserve_opn_voices(bank, 1);
	if(!voice) return -1;
	voice->lfo = 0;
	voice->slot = 0x0f;
	voice->fb_con = (f.fb & 0x07) << 3 | (f.alg & 0x07);
	for(int i = 0; i < 4; i++) {
		struct opn_voice_operator *op = &voice->operators[i];
		struct tfi_file_operator *fop = &f.operators[i];
		op->dt_mul = (fop->dt & 0x07) << 3 | (fop->mul & 0x07);
		op->tl = fop->tl & 0x7f;
		op->ks_ar = fop->rs << 6 | (fop->ar & 0x1f);
		op->am_dr = fop->dr & 0x1f;
		op->sr = fop->sr;
		op->sl_rr = fop->sl << 4 | (fop->rr & 0x0f);
		op->ssg_eg = fop->ssg_eg;
	}
	return 0;
}

static int save(struct fm_voice_bank *bank, struct fm_voice_bank_position *pos, int (*write_fn)(void *, size_t, void *), void *data_ptr) {
	if(bank->num_opn_voices <= pos->opn) return -1;	
	struct tfi_file f;
	f.fb = bank->opn_voices[pos->opn].fb_con >> 3;
	f.alg = bank->opn_voices[pos->opn].fb_con & 0x07;
	for(int i = 0; i < 4; i++) {
		f.operators[i].dt = bank->opn_voices[pos->opn].operators[i].dt_mul >> 3;
		f.operators[i].mul = bank->opn_voices[pos->opn].operators[i].dt_mul & 0x07;
		f.operators[i].tl = opn_voice_get_operator_tl(bank->opn_voices + pos->opn, i);
		f.operators[i].rs = bank->opn_voices[pos->opn].operators[i].ks_ar >> 6;
		f.operators[i].ar = bank->opn_voices[pos->opn].operators[i].ks_ar & 0x1f;
		f.operators[i].dr = bank->opn_voices[pos->opn].operators[i].am_dr;
		f.operators[i].sr = opn_voice_get_operator_sr(bank->opn_voices + pos->opn, i);
		f.operators[i].sl = bank->opn_voices[pos->opn].operators[i].sl_rr >> 4;
		f.operators[i].rr = bank->opn_voices[pos->opn].operators[i].sl_rr & 0x0f;
	}
	pos->opn++;
	return tfi_file_save(&f, write_fn, data_ptr);
}

struct loader tfi_file_loader = {
	.load = load,
	.save = save,
	.name = "TFI",
	.description = "TFI",
	.file_ext = "tfi",
	.max_opl_voices = 0,
	.max_opm_voices = 0,
	.max_opn_voices = 1,
};

#endif
