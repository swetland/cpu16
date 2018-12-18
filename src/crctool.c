// Copyright 2015, Brian Swetland <swetland@frotz.net>
// Licensed under the Apache License, Version 2.0.

// Based on "A Practical Parallel CRC Generation Method" by Evgeni Stavinov,
// published in Circuit Cellar, January 2010

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

uint32_t serial_crc(uint32_t crc, uint32_t poly, size_t nbits, uint32_t din) {
	din = din ^ ((crc >> (nbits-1)) & 1);
	crc <<= 1;
	if (din) crc ^= poly;
	return crc & (0xFFFFFFFF >> (32-nbits));
}

uint32_t parallel_crc(uint32_t crc, uint32_t poly, size_t nbits, uint32_t din, size_t dbits, int LSB) {
	for (size_t n = 0; n < dbits; n++) {
		if (LSB) {
			crc = serial_crc(crc, poly, nbits, din & 1);
			din >>= 1;
		} else {
			crc = serial_crc(crc, poly, nbits, (din >> (dbits-1)) & 1);
			din <<= 1;
		}
	}
	return crc;
}

char* binary(uint32_t n, size_t bits) {
	static char out[33];
	char *p = out;
	while (bits-- > 0) {
		*p++ = '0' + ((n >> bits) & 1);
	}
	*p = 0;
	return out;
}

int main(int argc, char** argv) {
	size_t crcsz = 5;
	uint32_t poly = 5;
	size_t dinsz = 4;
	int reverse = 0;
	int invert = 0;
	int lsb = 0;

	while (argc > 1) {
		if (!strcmp(argv[1], "-help")) {
			fprintf(stderr, "usage: crctool -poly=<hex> -crcsz=<bits> -dinsz=<bits> [-reverse] [-invert]\n");
			return 0;
		} else if (!strcmp(argv[1], "-reverse")) {
			reverse = 1;
		} else if (!strcmp(argv[1], "-invert")) {
			invert = 1;
		} else if (!strncmp(argv[1], "-poly=", 6)) {
			poly = strtoul(argv[1] + 6, NULL, 16);
		} else if(!strncmp(argv[1], "-crcsz=", 7)) {
			crcsz = strtoul(argv[1] + 7, NULL, 10);
		} else if(!strncmp(argv[1], "-dinsz=", 7)) {
			dinsz = strtoul(argv[1] + 7, NULL, 10);
		} else {
			fprintf(stderr, "error: unknown option: %s\n", argv[1]);
			return -1;
		}
		argv++;
		argc--;
	}
	if ((crcsz < 2) || (crcsz > 32)) {
		fprintf(stderr, "error: crc size %zu unsupported\n", crcsz);
		return -1;
	}
	if ((dinsz < 1) || (dinsz > crcsz)) {
		fprintf(stderr, "error: din size %zu unsupported\n", dinsz);
		return -1;
	}

	uint32_t h1[32];
	for (size_t n = 0; n < dinsz; n++) {
		h1[n] = parallel_crc(0, poly, crcsz, 1 << n, dinsz, lsb);
		//fprintf(stderr, "h1[%zu] = %s\n", n, binary(h1[n], crcsz));
	}

	uint32_t h2[32];
	for (size_t n = 0; n < crcsz; n++) {
		h2[n] = parallel_crc(1 << n, poly, crcsz, 0, dinsz, lsb);
		//fprintf(stderr, "h2[%zu] = %s\n", n, binary(h2[n], crcsz));
	}

	printf("// autogenerated by crctool -poly=%x -crcsz=%zu -dinsz=%zu%s%s\n\n",
		poly, crcsz, dinsz, reverse ? " -reverse" : "", invert ? " -invert" : "");
	printf("module crc_%zu_%x_%zu(\n", crcsz, poly, dinsz);
	printf("\tinput clk,\n");
	printf("\tinput rst,\n");
	printf("\tinput en,\n");
	printf("\tinput [%zu:0]din,\n", dinsz - 1);
	printf("\toutput [%zu:0]crc\n", crcsz - 1);
	printf("\t);\n" "\n" "reg [%zu:0]c;\n" "reg [%zu:0]n;\n" "\n", crcsz - 1, crcsz - 1);
	if (reverse) {
		printf("wire [%zu:0]d = {din[0]", dinsz - 1);
		for (size_t n = 1; n < dinsz; n++) {
			printf(",din[%zu]", n);
		}
		printf("};\n\n");
		printf("assign crc = %s{\n\tc[0]", invert ? "~" : "");
		for (size_t n = 1; n < crcsz; n++) {
			if ((n % 8) == 0) printf("\n\t");
			printf(",c[%zu]", n);
		}
		printf("\n\t};\n\n");
	} else {
		printf("wire [%zu:0]d = din;\n\n", dinsz - 1);
		printf("assign crc = %sc;\n\n", invert ? "~" : "");
	}
	printf("always_comb begin\n");
	for (size_t n = 0; n < crcsz; n++) {
		char* op = " = ";
		printf("\tn[%zu]", n);
		uint32_t bit = 1 << n;
		for (size_t i = 0; i < crcsz; i++) {
			if (h2[i] & bit) {
				printf("%sc[%zu]", op, i);
				op = "^";
			}
		}
		for (size_t i = 0; i < dinsz; i++) {
			if (h1[i] & bit) {
				printf("%sd[%zu]", op, i);
				op = "^";
			}
		}
		printf(";\n");
	}

	printf(
"end\n"
"\n"
"always_ff @(posedge clk) begin\n"
"\tif (rst) begin\n"
"\t\tc <= %zu'h%X;\n"
"\tend else if (en) begin\n"
"\t\tc <= n;\n"
"\tend\n"
"end\n"
"\n"
"endmodule\n", crcsz, 0xFFFFFFFF >> (32 - crcsz));
	return 0;
}
