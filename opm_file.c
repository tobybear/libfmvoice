#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "opm_file.h"

void opm_file_init(struct opm_file *f) {
	for(int i = 0; i < OPM_FILE_MAX_VOICES; i++) {
		struct opm_file_voice *v = f->voices + i;
		v->lfo_lfrq = 0;
		v->lfo_amd = 0;
		v->lfo_pmd = 0;
		v->lfo_wf = 0;
		v->nfrq = 0;
		v->ch_pan = 64;
		v->ch_fl = 7;
		v->ch_con = 4;
		v->ch_ams = 0;
		v->ch_pms = 0;
		v->ch_slot = 120;
		v->ch_ne = 0;
		for(int j = 0; j < 4; j++) {
			struct opm_file_operator *o = v->operators + j;
			o->ar = 31;
			o->d1r = 0;
			o->d2r = 0;
			o->rr = 4;
			o->d1l = 0;
			o->tl = 0;
			o->ks = 0;
			o->mul = 1;
			o->dt1 = 0;
			o->dt2 = 0;
			o->ame = 0;
		}
	}
}

int opm_file_load(struct opm_file *opm, uint8_t *data, size_t data_len) {
	memset(opm, 0, sizeof(struct opm_file));
	if(data_len < 30) return -1;

#define ALL_STATES \
	STATE(None) \
	STATE(Slash) \
	STATE(Comment) \
	STATE(InAt) \
	STATE(AfterAt) \
	STATE(InVoiceNum) \
	STATE(AfterVoiceNum) \
	STATE(InVoiceName) \
	STATE(InParamName) \
	STATE(AfterParamName) \
	STATE(InParamVal) \
	STATE(AfterParamVal)

	enum {
#define STATE(x) x,
ALL_STATES
#undef STATE
	} state = None;

#define CHAR_DOT(x) ((x) < 0x20 || (x) >= 0x7f ? '.' : (x))

#define MAX_PARAM_NAME 4
#define MAX_DIGITS 4
#define MAX_PARAMS 11
#define MAX_VOICE_NAME 256
#define APPEND_DIGIT { if(digitNum >= MAX_DIGITS) { fprintf(stderr, "Too many digits %d '%c' (%d) at line %d\n", digitNum, CHAR_DOT(c), c, line); return -1; } else { digits[digitNum++] = c; } }
#define APPEND_PARAM { if(paramNum >= MAX_PARAMS) { fprintf(stderr, "Too many parameters at line %d\n", line); return -1; } else { digits[digitNum] = 0; params[paramNum++] = atoi(digits); digitNum = 0; } }
#define APPEND_VOICE_NAME_CHAR { if(voiceNameLen >= MAX_VOICE_NAME) { fprintf(stderr, "Too many voice chars '%c' (%d) at line %d\n", CHAR_DOT(c), c, line); return -1; } else { voiceName[voiceNameLen++] = c; } }
#define PARSE_PARAM { \
	if(curVoice < OPM_FILE_MAX_VOICES && curVoice >= 0) { \
		if(!strcmp(paramName, "LFO")) { \
			if(paramNum != 5) { \
				fprintf(stderr, "LFO parameters %d != 5\n", paramNum); \
				return -1; \
			} \
			opm->voices[curVoice].lfo_lfrq = params[0]; \
			opm->voices[curVoice].lfo_amd = params[1]; \
			opm->voices[curVoice].lfo_pmd = params[2]; \
			opm->voices[curVoice].lfo_wf = params[3]; \
			opm->voices[curVoice].nfrq = params[4]; \
		} else if(!strcmp(paramName, "CH")) { \
			if(paramNum != 7) { \
				fprintf(stderr, "CH parameters %d != 7\n", paramNum); \
				return -1; \
			} \
			opm->voices[curVoice].ch_pan = params[0]; \
			opm->voices[curVoice].ch_fl = params[1]; \
			opm->voices[curVoice].ch_con = params[2]; \
			opm->voices[curVoice].ch_ams = params[3]; \
			opm->voices[curVoice].ch_pms = params[4]; \
			opm->voices[curVoice].ch_slot = params[5]; \
			opm->voices[curVoice].ch_ne = params[6]; \
		} else if((paramName[0] == 'M' || paramName[0] == 'C') && (paramName[1] == '1' || paramName[1] == '2')) { \
			if(paramNum != 11) { \
				fprintf(stderr, "Operator %c%c parameters %d != 11\n", paramName[0], paramName[1], paramNum); \
				return -1; \
			} \
			int opNum = paramName[0] == 'M' ? (paramName[1] == '1' ? 0 : 2) : (paramName[1] == '1' ? 1 : 3); \
			opm->voices[curVoice].operators[opNum].ar = params[0]; \
			opm->voices[curVoice].operators[opNum].d1r = params[1]; \
			opm->voices[curVoice].operators[opNum].d2r = params[2]; \
			opm->voices[curVoice].operators[opNum].rr = params[3]; \
			opm->voices[curVoice].operators[opNum].d1l = params[4]; \
			opm->voices[curVoice].operators[opNum].tl = params[5]; \
			opm->voices[curVoice].operators[opNum].ks = params[6]; \
			opm->voices[curVoice].operators[opNum].mul = params[7]; \
			opm->voices[curVoice].operators[opNum].dt1 = params[8]; \
			opm->voices[curVoice].operators[opNum].dt2 = params[9]; \
			opm->voices[curVoice].operators[opNum].ame = params[10]; \
		} \
	} \
}

	int line = 1;
	char paramName[MAX_PARAM_NAME + 1];
	int paramNameLen = 0;
	char voiceName[MAX_VOICE_NAME];
	int voiceNameLen = 0;
	char digits[MAX_DIGITS + 1];
	int digitNum = 0;
	int params[MAX_PARAMS + 1];
	int paramNum = 0;
	int curVoice = -1;
	for(int i = 0; i < data_len; i++) {
		uint8_t c = data[i];
		//printf("line=%d c=%02x (%c) state=%d %s\n", line, c, c < 0x20 ? '.' : c, state, state_names[state]);

		if(c == '\r') continue;

		if(c == '\n') {
			line++;
		} else if(c == '/') {
			if(state != Comment)
				state = Slash;
		}

		switch(state) {
			case None:
				if(isspace(c)) {
					// ignore
				} else switch(c) {
					case '@':
						state = InAt;
						break;
					case 'L':
					case 'C':
					case 'M':
						state = InParamName;
						paramNameLen = 1;
						paramName[0] = c;
						break;
					default:
						fprintf(stderr, "Unexpected '%c' (%d) at start of line %d\n", CHAR_DOT(c), c, line);
						return -1;
						break;
				}
				break;
			case Slash:
				if(c == '/')
					state = Comment;
				else {
					fprintf(stderr, "Unexpected '%c' (%d) after '/' at line %d\n", CHAR_DOT(c), c, line);
					return -1;
				}
				break;
			case Comment:
				if(c == '\n')
					state = None;
				break;
			case InParamName:
				if(c == '\n') {
					fprintf(stderr, "Unexpected newline in parameter name at line %d\n", line-1);
					return -1;
				} else if(c == ':') {
					paramName[paramNameLen] = 0;
					state = AfterParamName;
				} else if(paramNameLen >= MAX_PARAM_NAME) {
					fprintf(stderr, "Unexpected '%c' (%d) in parameter name at line %d\n", CHAR_DOT(c), c, line);
					return -1;
				} else {
					paramName[paramNameLen++] = c;
				}
				break;
			case AfterParamName:
				if(isspace(c)) {
					// ignore
				} else if(isdigit(c)) {
					paramNum = 0;
					digits[0] = 0;
					digitNum = 0;
					APPEND_DIGIT;
					state = InParamVal;
				} else {
					fprintf(stderr, "Unexpected '%c' (%d) after parameter name at line %d\n", CHAR_DOT(c), c, line);
					return -1;
				}
				break;
			case InParamVal:
				if(c == '\n') {
					APPEND_PARAM;
					PARSE_PARAM;
					paramNum = 0;
					state = None;
				} else if(isspace(c)) {
					APPEND_PARAM;
					state = AfterParamVal;
				} else if(isdigit(c)) {
					APPEND_DIGIT;
				} else {
					fprintf(stderr, "Unexpected '%c' (%d) in parameter value at line %d\n", CHAR_DOT(c), c, line);
					return -1;
				}
				break;
			case AfterParamVal:
				if(c == '\n') {
					PARSE_PARAM;
					paramNum = 0;
					state = None;
				} else if(isspace(c)) {
					// do nothing
				} else if(isdigit(c)) {
					APPEND_DIGIT;
					state = InParamVal;
				}
				break;
			case InAt:
				if(isspace(c)) {
					// ignore
				} else if(c == ':') {
					state = AfterAt;
				}
				break;
			case AfterAt:
				if(isspace(c)) {
					//ignore
				} else if(isdigit(c)) {
					digitNum = 0;
					APPEND_DIGIT;
					state = InVoiceNum;
				} else {
					fprintf(stderr, "Unexpected '%c' (%d) after '@' at line %d\n", CHAR_DOT(c), c, line);
					return -1;
				}
				break;
			case InVoiceNum:
				if(isspace(c)) {
					digits[digitNum] = 0;
					curVoice = atoi(digits);
					if(curVoice > 255) curVoice = -1;
					state = AfterVoiceNum;
				} else if(isdigit(c)) {
					APPEND_DIGIT;
				} else {
					APPEND_VOICE_NAME_CHAR;
					state = InVoiceName;
				}
				break;
			case AfterVoiceNum:
				if(isspace(c)) {
					// ignore
				} else {
					APPEND_VOICE_NAME_CHAR;
					state = InVoiceName;
				}
				break;
			case InVoiceName:
				if(c == '\n') {
					voiceName[voiceNameLen] = 0;
					if(curVoice < OPM_FILE_MAX_VOICES && curVoice >= 0) {
						opm->voices[curVoice].used = 1;
						strncpy(opm->voices[curVoice].name, voiceName, voiceNameLen);
						opm->voices[curVoice].name[voiceNameLen] = 0;
					}
					voiceNameLen = 0;
					state = None;
				} else {
					APPEND_VOICE_NAME_CHAR;
				}
				break;
		}
	}

	/* Mark "no Name" voices as unused */
	for(int i = 0; i < OPM_FILE_MAX_VOICES; i++) {
		struct opm_file_voice *v = &opm->voices[i];

		/* Don't mark already unused voices as unused */
		if(!v->used)
			continue;

		int unused = 1;

		for(int k = 0; k < 4; k++) {
			if(
				v->operators[k].ar != 31 ||
				v->operators[k].d1r != 0 ||
				v->operators[k].d2r != 0 ||
				v->operators[k].rr != 4 ||
				v->operators[k].d1l != 0 ||
				v->operators[k].tl != 0 ||
				v->operators[k].ks != 0 ||
				v->operators[k].mul != 1 ||
				v->operators[k].dt1 != 0 ||
				v->operators[k].dt2 != 0 ||
				v->operators[k].ame != 0
			) {
				unused = 0;
				break;
			}
		}

		if(unused &&
			v->lfo_lfrq == 0 &&
			v->lfo_amd == 0 &&
			v->lfo_pmd == 0 &&
			v->lfo_wf == 0 &&
			v->nfrq == 0 &&
			v->ch_pan == 64 &&
			v->ch_fl == 0 &&
			v->ch_con == 0 &&
			v->ch_ams == 0 &&
			v->ch_pms == 0 &&
			v->ch_slot == 64 &&
			v->ch_ne == 0
		) {
			opm->voices[i].used = 0;
		}
	}

	return 0;
}

static const char *opm_file_operator_name(int i) {
	const char *names[] = { "M1", "C1", "M2", "C2" };
	if(i > 3) return "??";
	return names[i];
}

int opm_file_save(struct opm_file *f, size_t (*write_fn)(void *buf, size_t bufsize, void *data_ptr), void *data_ptr) {
	uint8_t buf[256];

#define WRITEF(args...) { \
	int w = snprintf((char *)buf, sizeof(buf), args); \
	if(w < 0) return -1; \
	size_t wr = write_fn(buf, w, data_ptr); \
	if(wr < w) return -2; \
}

	WRITEF("//MiOPMdrv sound bank Paramer Ver2002.04.22\n");
	WRITEF("//LFO: LFRQ AMD PMD WF NFRQ\n");
	WRITEF("//@:[Num] [Name]\n");
	WRITEF("//CH: PAN	FL CON AMS PMS SLOT NE\n");
	WRITEF("//[OPname]: AR D1R D2R	RR D1L	TL	KS MUL DT1 DT2 AMS-EN\n");

	for(int i = 0; i < OPM_FILE_MAX_VOICES; i++) {
		struct opm_file_voice *v = f->voices + i;
		WRITEF("@:%d %s\n", i, v->name);
		WRITEF("LFO: %d %d %d %d %d\n", v->lfo_lfrq, v->lfo_amd, v->lfo_pmd, v->lfo_wf, v->nfrq);
		WRITEF("CH: %d %d %d %d %d %d %d\n", v->ch_pan, v->ch_fl, v->ch_con, v->ch_ams, v->ch_pms, v->ch_slot, v->ch_ne);
		for(int j = 0; j < 4; j++) {
			struct opm_file_operator *o = v->operators + j;
			WRITEF("%s: %d %d %d %d %d %d %d %d %d %d %d\n", opm_file_operator_name(i), o->ar, o->d1r, o->d2r, o->rr, o->d1l, o->tl, o->ks, o->mul, o->dt1, o->dt2, o->ame);
		}
	}

#undef WRITEF
	return 0;
}
