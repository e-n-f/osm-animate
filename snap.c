#include <stdio.h>
#include <stdlib.h>
#include <expat.h>
#include <limits.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <math.h>
#include <time.h>

struct node {
	unsigned id; // still just over 2^31
	int lat;
	int lon;
};

int nodecmp(const void *v1, const void *v2) {
	const struct node *n1 = v1;
	const struct node *n2 = v2;

	if (n1->id < n2->id) {
		return -1;
	}
	if (n1->id > n2->id) {
		return 1;
	}

	return 0;
}

// http://www.tbray.org/ongoing/When/200x/2003/03/22/Binary
void *search(const void *key, const void *base, size_t nel, size_t width,
		int (*cmp)(const void *, const void *)) {

	long long high = nel, low = -1, probe;
	while (high - low > 1) {
		probe = (low + high) >> 1;
		int c = cmp(((char *) base) + probe * width, key);
		if (c > 0) {
			high = probe;
		} else {
			low = probe;
		}
	}

	if (low < 0) {
		low = 0;
	}

	return ((char *) base) + low * width;
}

// boilerplate from
// http://marcomaggi.github.io/docs/expat.html#overview-intro
// Copyright 1999, Clark Cooper

#define BUFFSIZE        8192

char tmpfname[L_tmpnam];
FILE *tmp;
void *map = NULL;
long long nel;

unsigned theway = 0;
char thetimestamp[5000] = "";
struct node *thenodes[100000];
unsigned thenodecount = 0;
long long seq = 0;

char tags[50000] = "";

static void XMLCALL start(void *data, const char *element, const char **attribute) {
	static struct node prevnode = { 0, 0, 0 };

	if (strcmp(element, "node") == 0) {
		struct node n;
		int i;
		n.id = 0;
		n.lat = INT_MIN;
		n.lon = INT_MIN;

		for (i = 0; attribute[i] != NULL; i += 2) {
			if (strcmp(attribute[i], "id") == 0) {
				n.id = atoi(attribute[i + 1]);
			} else if (strcmp(attribute[i], "lat") == 0) {
				n.lat = atof(attribute[i + 1]) * 1000000.0;
			} else if (strcmp(attribute[i], "lon") == 0) {
				n.lon = atof(attribute[i + 1]) * 1000000.0;
			}
		}

		if (nodecmp(&n, &prevnode) < 0) {
			fprintf(stderr, "node went backwards (%d): ",
				nodecmp(&n, &prevnode));
			fprintf(stderr, "%u %d %d to ", prevnode.id, prevnode.lat, prevnode.lon);
			fprintf(stderr, "%u %d %d\n", n.id, n.lat, n.lon);
		} else {
			fwrite(&n, sizeof(struct node), 1, tmp);
			prevnode = n;
		}

		if (seq++ % 100000 == 0) {
			fprintf(stderr, "node %u  \r", n.id);
		}
	} else if (strcmp(element, "way") == 0) {
		if (map == NULL) {
			fflush(tmp);

			int fd = open(tmpfname, O_RDONLY);
			if (fd < 0) {
				perror(tmpfname);
				exit(EXIT_FAILURE);
			}

			struct stat st;
			if (fstat(fd, &st) < 0) {
				perror("stat");
				exit(EXIT_FAILURE);
			}

			nel = st.st_size / sizeof(struct node);

			fprintf(stderr, "%d, %lld\n", fd, (long long) st.st_size);

			map = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
			if (map == MAP_FAILED) {
				perror("mmap");
				exit(EXIT_FAILURE);
			}

			close(fd);
		}

		int i;
		thenodecount = 0;
		strcpy(thetimestamp, "");
		strcpy(tags, "");

		for (i = 0; attribute[i] != NULL; i += 2) {
			if (strcmp(attribute[i], "id") == 0) {
				theway = atoi(attribute[i + 1]);
			}
			if (strcmp(attribute[i], "timestamp") == 0) {
				strcpy(thetimestamp, attribute[i + 1]);
			}
		}
	} else if (strcmp(element, "nd") == 0) {
		int i;

		struct node n;
		n.id = 0;

		for (i = 0; attribute[i] != NULL; i += 2) {
			if (strcmp(attribute[i], "ref") == 0) {
				n.id = atoi(attribute[i + 1]);
			}
		}

		struct node *find = search(&n, map, nel, sizeof(struct node), nodecmp);

		if (find->lat == INT_MIN) {
			fprintf(stderr, "FAIL looked for %u found %u %d\n", n.id, find->id, find->lat);
		} else if (find->id == n.id) {
			thenodes[thenodecount++] = find;
		} else {
			fprintf(stderr, "FAIL looked for %u found %u\n", n.id, find->id);
		}
	} else if (strcmp(element, "tag") == 0) {
		if (theway != 0) {
			const char *key = "";
			const char *value = "";

			int i;
			for (i = 0; attribute[i] != NULL; i += 2) {
				if (strcmp(attribute[i], "k") == 0) {
					key = attribute[i + 1];
				}
				if (strcmp(attribute[i], "v") == 0) {
					value = attribute[i + 1];
				}
			}

			int n = strlen(tags);
			if (n + strlen(key) + strlen(value) + 5 < sizeof(tags)) {
				sprintf(tags + n, ";%s=%s", key, value);
			}
		}
	}
}

int max = 10;

static void XMLCALL end(void *data, const char *el) {
	if (strcmp(el, "way") == 0) {
		int x;
		struct tm tm;
		time_t t = 0;

		if (sscanf(thetimestamp, "%d-%d-%dT%d:%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec) == 6) {
			tm.tm_isdst = -1;
			tm.tm_year -= 1900;
			tm.tm_mon -= 1;
			t = mktime(&tm);
		} else {
			fprintf(stderr, "couldn't parse %s\n", thetimestamp);
		}

		for (x = 0; x < thenodecount; x += max - 1) {
			if (x + 1 < thenodecount) {
				int i;
				for (i = x; i < x + max && i < thenodecount; i++) {
					printf("%lf,%lf ", thenodes[i]->lat / 1000000.0,
							   thenodes[i]->lon / 1000000.0);
				}

				printf(" 32:%d", (int) t);

				printf("// id=%u ", theway);
				printf("// timestamp=%s ", thetimestamp);
				printf("%s\n", tags);
			}
		}

		theway = 0;
	}
}

int main(int argc, char *argv[]) {
	int i;
	extern int optind;
	extern char *optarg;

	while ((i = getopt(argc, argv, "s:")) != -1) {
		switch (i) {
		case 's':
			max = atoi(optarg);
			if (max == 0) {
				max = INT_MAX / 2;
			}
			break;

		default:
			fprintf(stderr, "Usage: %s [-s num]\n", argv[0]);
			exit(EXIT_FAILURE);
		}
	}
	
	if (tmpnam(tmpfname) == NULL) {
		perror(tmpfname);
		exit(EXIT_FAILURE);
	}

	tmp = fopen(tmpfname, "w");
	if (tmp == NULL) {
		perror(tmpfname);
		exit(1);
	}

	XML_Parser p = XML_ParserCreate(NULL);
	if (p == NULL) {
		fprintf(stderr, "Couldn't allocate memory for parser\n");
		exit(EXIT_FAILURE);
	}

	XML_SetElementHandler(p, start, end);

	int done = 0;
	while (!done) {
		int len;
		char Buff[BUFFSIZE];

		len = fread(Buff, 1, BUFFSIZE, stdin);
		if (ferror(stdin)) {
       			fprintf(stderr, "Read error\n");
			exit(EXIT_FAILURE);
		}
		done = feof(stdin);

		if (XML_Parse(p, Buff, len, done) == XML_STATUS_ERROR) {
			fprintf(stderr, "Parse error at line %lld:\n%s\n", (long long) XML_GetCurrentLineNumber(p), XML_ErrorString(XML_GetErrorCode(p)));
			exit(EXIT_FAILURE);
		}
	}

	XML_ParserFree(p);
	unlink(tmpfname);
	return 0;
}
