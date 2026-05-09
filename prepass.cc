#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define LEN(a) (sizeof((a)) / sizeof(*(a)))

template <typename T> struct ConstifyImpl {
    using V = const T;
};

template <typename T> struct ConstifyImpl<const T> {
    using V = const T;
};

template <typename T> using Constify = typename ConstifyImpl<T>::V;

template <typename T, typename Size = unsigned int> struct Slice {
    Size len;
    T *dat;

    T &operator[](Size n) {
        assert(n < len);
        return dat[n];
    }

    Slice from(Size start) {
        assert(start <= len);

        Slice res = {};
        if (start < len) {
            res.len = len - start;
            res.dat = dat + start;
        }
        return res;
    }

    Slice until(Size end) {
        assert(end <= len);
        return {end, dat};
    }

    Slice<Constify<T>, Size> constify() const { return {len, (T const *)dat}; }
};

struct Reader {
    FILE *file;
    unsigned int cap;
    char *buf;

    unsigned int fill;
    unsigned int scan;
    unsigned int head;
    bool finished;
};

#define READER_FROM(file, buf) readerFrom(file, LEN(buf), buf)
Reader readerFrom(FILE *file, unsigned int cap, char *buf) {
    Reader reader = {};

    reader.file = file;
    reader.cap = cap;
    reader.buf = buf;

    return reader;
}

Slice<char> readLine(Reader &reader) {
    bool retry = false;
    Slice<char> res = {};

    if (reader.scan == reader.fill) {
        if (!reader.finished) {
            if (reader.head > 0) {
                // rebase
                unsigned long to_move = reader.scan - reader.head;
                memmove(reader.buf, reader.buf + reader.head, to_move);

                reader.fill -= reader.head;
                reader.scan -= reader.head;
                reader.head = 0;
            } else if (reader.fill == reader.cap) {
                assert(!"Buff too small");
            }

            unsigned int rem = reader.cap - reader.fill;
            unsigned long nread =
                fread(reader.buf + reader.fill, 1, rem, reader.file);

            assert(nread == (unsigned long)(unsigned int)nread);

            if (nread == 0) {
                reader.finished = true;
            } else {
                reader.fill += nread;
            }

            retry = true;
        } else {
            unsigned int len = reader.scan - reader.head;
            char *start = reader.buf + reader.head;

            res = {len, start};

            reader.head += len;
        }
    } else {
        bool found = false;
        while (!found && reader.scan < reader.fill) {
            char c = reader.buf[reader.scan];
            if (c == '\n') {
                unsigned int len = reader.scan - reader.head;
                char *start = reader.buf + reader.head;

                res = {len, start};

                reader.head += len + 1; // skip the new line
                reader.scan += 1;

                found = true;

                break;
            } else {
                reader.scan += 1;
            }
        }

        if (!found) {
            retry = true;
        }
    }

    if (retry) {
        return readLine(reader);
    }

    return res;
}

struct Writer {
    FILE *file;
    unsigned int cap;
    char *buf;

    unsigned int fill;
};

#define WRITER_FROM(file, buf) writerFrom(file, LEN(buf), buf)
Writer writerFrom(FILE *file, unsigned int cap, char *buf) {
    Writer writer = {};

    writer.file = file;
    writer.cap = cap;
    writer.buf = buf;

    return writer;
}

void writeStr(Writer &writer, char const *s);
void writeChar(Writer &writer, char c);
void writeSlice(Writer &writer, Slice<char const> s);

void flush(Writer &writer) {
    if (writer.fill > 0) {
        unsigned long written = fwrite(writer.buf, writer.fill, 1, writer.file);
        assert(written == 1);

        writer.fill = 0;
    }
}

void writeStr(Writer &writer, char const *s) {
    unsigned long lslen = strlen(s);
    assert(lslen == (unsigned long)(unsigned int)lslen);
    unsigned int slen = (unsigned int)lslen;

    Slice<char const> slice = {slen, s};
    writeSlice(writer, slice);
}

void writeChar(Writer &writer, char c) {
    Slice<char const> slice = {1, &c};
    writeSlice(writer, slice);
}

void writeSlice(Writer &writer, Slice<char const> s) {
    if (s.len > 0) {
        unsigned int rem = writer.cap - writer.fill;
        if (rem < s.len) {
            Slice<char const> first = s.until(rem);

            writeSlice(writer, first);
            flush(writer);
            writeSlice(writer, s.from(rem));
        } else {
            memcpy(writer.buf + writer.fill, s.dat, s.len);
            writer.fill += s.len;
        }
    }
}

int main(int argc, char const **argv) {
#define BUF_LEN 1024

    assert(argc == 3);
    char const *in_name = argv[1];
    char const *out_name = argv[2];

    FILE *in_file = fopen(in_name, "r");
    if (in_file == nullptr) {
        return 1;
    }

    FILE *out_file = fopen(out_name, "w");
    if (out_file == nullptr) {
        fclose(in_file);
        return 1;
    }

    char read_buf[BUF_LEN];
    Reader reader = READER_FROM(in_file, read_buf);

    char write_buf[BUF_LEN];
    Writer writer = WRITER_FROM(out_file, write_buf);

    char const *preamble = R"""(
#ifndef GL_FUNCTIONS_DEF
#define GL_FUNCTIONS_DEF

#include <GL/gl.h>
#include <GL/glext.h>

#define FOR_GL_FUNCTIONS(X))""";

    char const *postamble = R"""(

#endif // GL_FUNCTIONS_DEF
)""";

    writeStr(writer, preamble + 1); // skip initial newline

    Slice<char> line = {};
    while ((line = readLine(reader)).len != 0) {
        assert(line.len > 2);
        assert(line[0] == 'g');
        assert(line[1] == 'l');
        assert(isupper(line[2]));

        writeStr(writer, "\\\n    X(");
        writeSlice(writer, line.constify());
        writeStr(writer, ", ");

        Slice<char> mapped = line.from(2);
        mapped[0] = tolower(mapped[0]);

        writeSlice(writer, mapped.constify());
        writeChar(writer, ')');
    }

    writeStr(writer, postamble);
    flush(writer);

    fclose(out_file);
    fclose(in_file);

    return 0;
}
