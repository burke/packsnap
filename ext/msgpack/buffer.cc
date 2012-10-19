/*
 * MessagePack for Ruby
 *
 * Copyright (C) 2008-2012 FURUHASHI Sadayuki
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "snappy.h"
#include "buffer.hh"
#include "rmem.h"

#ifdef RUBY_VM
#define HAVE_RB_STR_REPLACE
#endif

#ifndef HAVE_RB_STR_REPLACE
static ID s_replace;
#endif

#ifndef DISABLE_RMEM
static msgpack_rmem_t s_rmem;
#endif

void msgpack_buffer_static_init()
{
#ifndef DISABLE_RMEM
    msgpack_rmem_init(&s_rmem);
#endif
#ifndef HAVE_RB_STR_REPLACE
    s_replace = rb_intern("replace");
#endif
}

void msgpack_buffer_static_destroy()
{
#ifndef DISABLE_RMEM
    msgpack_rmem_destroy(&s_rmem);
#endif
}

void msgpack_buffer_init(msgpack_buffer_t* b)
{
    memset(b, 0, sizeof(msgpack_buffer_t));

    b->head = &b->tail;
    b->write_reference_threshold = MSGPACK_BUFFER_STRING_WRITE_REFERENCE_DEFAULT;
    b->read_reference_threshold = MSGPACK_BUFFER_STRING_READ_REFERENCE_DEFAULT;
    b->io_buffer_size = MSGPACK_BUFFER_IO_BUFFER_SIZE_DEFAULT;
    b->io = Qnil;
    b->io_buffer = Qnil;
}

static void _msgpack_buffer_chunk_destroy(msgpack_buffer_chunk_t* c)
{
    if(c->mem != NULL) {
#ifndef DISABLE_RMEM
        if(!msgpack_rmem_free(&s_rmem, c->mem)) {
            free(c->mem);
        }
        /* no needs to update rmem_owner because chunks will not be
         * free()ed and thus *rmem_owner = NULL is always valid. */
#else
        free(c->mem);
#endif
    }
    c->first = NULL;
    c->last = NULL;
    c->mem = NULL;
}

void msgpack_buffer_destroy(msgpack_buffer_t* b)
{
    /* head is always available */
    msgpack_buffer_chunk_t* c = b->head;
    while(c != &b->tail) {
        msgpack_buffer_chunk_t* n = c->next;
        _msgpack_buffer_chunk_destroy(c);
        free(c);
        c = n;
    }
    _msgpack_buffer_chunk_destroy(c);

    c = b->free_list;
    while(c != NULL) {
        msgpack_buffer_chunk_t* n = c->next;
        free(c);
        c = n;
    }
}

void msgpack_buffer_mark(msgpack_buffer_t* b)
{
    /* head is always available */
    msgpack_buffer_chunk_t* c = b->head;
    while(c != &b->tail) {
        rb_gc_mark(c->mapped_string);
        c = c->next;
    }
    rb_gc_mark(c->mapped_string);

    rb_gc_mark(b->io);
    rb_gc_mark(b->io_buffer);

    rb_gc_mark(b->owner);
}

bool _msgpack_buffer_shift_chunk(msgpack_buffer_t* b)
{
    _msgpack_buffer_chunk_destroy(b->head);

    if(b->head == &b->tail) {
        /* list becomes empty. don't add head to free_list
         * because head should be always available */
        b->tail_buffer_end = NULL;
        b->read_buffer = NULL;
        return false;
    }

    /* add head to free_list */
    msgpack_buffer_chunk_t* next_head = b->head->next;
    b->head->next = b->free_list;
    b->free_list = b->head;

    b->head = next_head;
    b->read_buffer = next_head->first;

    return true;
}

void msgpack_buffer_clear(msgpack_buffer_t* b)
{
    while(_msgpack_buffer_shift_chunk(b)) {
        ;
    }
}

size_t msgpack_buffer_read_to_string_nonblock(msgpack_buffer_t* b, VALUE string, size_t length)
{
    size_t avail = msgpack_buffer_top_readable_size(b);

#ifndef DISABLE_BUFFER_READ_REFERENCE_OPTIMIZE
    /* optimize */
    if(length <= avail && RSTRING_LEN(string) == 0 &&
            b->head->mapped_string != NO_MAPPED_STRING &&
            length >= b->read_reference_threshold) {
        VALUE s = _msgpack_buffer_refer_head_mapped_string(b, length);
#ifndef HAVE_RB_STR_REPLACE
        /* TODO MRI 1.8 */
        rb_funcall(string, s_replace, 1, s);
#else
        rb_str_replace(string, s);
#endif
        _msgpack_buffer_consumed(b, length);
        return length;
    }
#endif

    size_t const length_orig = length;

    while(true) {
//printf("avail: %lu\n", avail);
//printf("length: %lu\n", length);
        if(length <= avail) {
//printf("read_buffer: %p\n", b->read_buffer);
            rb_str_buf_cat(string, b->read_buffer, length);
            _msgpack_buffer_consumed(b, length);
            return length_orig;
        }

        rb_str_buf_cat(string, b->read_buffer, avail);
        length -= avail;

        if(!_msgpack_buffer_shift_chunk(b)) {
            return length_orig - length;
        }

        avail = msgpack_buffer_top_readable_size(b);
    }
}

size_t msgpack_buffer_read_nonblock(msgpack_buffer_t* b, char* buffer, size_t length)
{
    /* buffer == NULL means skip */
    size_t const length_orig = length;

    while(true) {
        size_t avail = msgpack_buffer_top_readable_size(b);

        if(length <= avail) {
            if(buffer != NULL) {
                memcpy(buffer, b->read_buffer, length);
            }
            _msgpack_buffer_consumed(b, length);
            return length_orig;
        }

        if(buffer != NULL) {
            memcpy(buffer, b->read_buffer, avail);
            buffer += avail;
        }
        length -= avail;

        if(!_msgpack_buffer_shift_chunk(b)) {
            return length_orig - length;
        }
    }
}

size_t msgpack_buffer_all_readable_size(const msgpack_buffer_t* b)
{
    size_t sz = msgpack_buffer_top_readable_size(b);

    if(b->head == &b->tail) {
        return sz;
    }

    msgpack_buffer_chunk_t* c = b->head->next;

    while(true) {
        sz += c->last - c->first;
        if(c == &b->tail) {
            return sz;
        }
        c = c->next;
    }
}

bool _msgpack_buffer_read_all2(msgpack_buffer_t* b, char* buffer, size_t length)
{
    if(!msgpack_buffer_ensure_readable(b, length)) {
        return false;
    }

    msgpack_buffer_read_nonblock(b, buffer, length);
    return true;
}


static inline msgpack_buffer_chunk_t* _msgpack_buffer_alloc_new_chunk(msgpack_buffer_t* b)
{
    msgpack_buffer_chunk_t* reuse = b->free_list;
    if(reuse == NULL) {
        return (msgpack_buffer_chunk_t*)malloc(sizeof(msgpack_buffer_chunk_t));
    }
    b->free_list = b->free_list->next;
    return reuse;
}

static inline void _msgpack_buffer_add_new_chunk(msgpack_buffer_t* b)
{
    if(b->head == &b->tail) {
        if(b->tail.first == NULL) {
            /* empty buffer */
            return;
        }

        msgpack_buffer_chunk_t* nc = _msgpack_buffer_alloc_new_chunk(b);

        *nc = b->tail;
        b->head = nc;
        nc->next = &b->tail;

    } else {
        /* search node before tail */
        msgpack_buffer_chunk_t* before_tail = b->head;
        while(before_tail->next != &b->tail) {
            before_tail = before_tail->next;
        }

        msgpack_buffer_chunk_t* nc = _msgpack_buffer_alloc_new_chunk(b);

#ifndef DISABLE_RMEM
#ifndef DISABLE_RMEM_REUSE_INTERNAL_FRAGMENT
        if(b->rmem_owner == &b->tail.mem) {
            /* reuse unused rmem */
            size_t unused = b->tail_buffer_end - b->tail.last;
            b->rmem_last -= unused;
        }
#endif
#endif

        /* rebuild tail */
        *nc = b->tail;
        before_tail->next = nc;
        nc->next = &b->tail;
    }
}

static inline void _msgpack_buffer_append_reference(msgpack_buffer_t* b, VALUE string)
{
    VALUE mapped_string = rb_str_dup(string);
#ifdef COMPAT_HAVE_ENCODING
    ENCODING_SET(mapped_string, s_enc_ascii8bit);
#endif

    _msgpack_buffer_add_new_chunk(b);

    char* data = RSTRING_PTR(string);
    size_t length =RSTRING_LEN(string);

    b->tail.first = (char*) data;
    b->tail.last = (char*) data + length;
    b->tail.mapped_string = mapped_string;
    b->tail.mem = NULL;

    /* msgpack_buffer_writable_size should return 0 for mapped chunk */
    b->tail_buffer_end = b->tail.last;

    /* consider read_buffer */
    if(b->head == &b->tail) {
        b->read_buffer = b->tail.first;
    }
}

void _msgpack_buffer_append_long_string(msgpack_buffer_t* b, VALUE string)
{
    size_t length = RSTRING_LEN(string);

    if(b->io != Qnil) {
        msgpack_buffer_flush(b);
        rb_funcall(b->io, b->io_write_all_method, 1, string);

    } else if(!STR_DUP_LIKELY_DOES_COPY(string)) {
        _msgpack_buffer_append_reference(b, string);

    } else {
        msgpack_buffer_append(b, RSTRING_PTR(string), length);
    }
}

static inline void* _msgpack_buffer_chunk_malloc(
        msgpack_buffer_t* b, msgpack_buffer_chunk_t* c,
        size_t required_size, size_t* allocated_size)
{
#ifndef DISABLE_RMEM
    if(required_size <= MSGPACK_RMEM_PAGE_SIZE) {
#ifndef DISABLE_RMEM_REUSE_INTERNAL_FRAGMENT
        if((size_t)(b->rmem_end - b->rmem_last) < required_size) {
#endif
            /* alloc new rmem page */
            *allocated_size = MSGPACK_RMEM_PAGE_SIZE;
            char* buffer = (char*)msgpack_rmem_alloc(&s_rmem);
            c->mem = buffer;

            /* update rmem owner */
            b->rmem_owner = &c->mem;
            b->rmem_last = b->rmem_end = buffer + MSGPACK_RMEM_PAGE_SIZE;

            return buffer;

#ifndef DISABLE_RMEM_REUSE_INTERNAL_FRAGMENT
        } else {
            /* reuse unused rmem */
            *allocated_size = (size_t)(b->rmem_end - b->rmem_last);
            char* buffer = b->rmem_last;
            b->rmem_last = b->rmem_end;

            /* update rmem owner */
            c->mem = *b->rmem_owner;
            *b->rmem_owner = NULL;
            b->rmem_owner = &c->mem;

            return buffer;
        }
#endif
    }
#else
    if(required_size < 72) {
        required_size = 72;
    }
#endif

    // TODO alignment?
    *allocated_size = required_size;
    void* mem = malloc(required_size);
    c->mem = mem;
    return mem;
}

static inline void* _msgpack_buffer_chunk_realloc(
        msgpack_buffer_t* b, msgpack_buffer_chunk_t* c,
        void* mem, size_t required_size, size_t* current_size)
{
    if(mem == NULL) {
        return _msgpack_buffer_chunk_malloc(b, c, required_size, current_size);
    }

    size_t next_size = *current_size * 2;
    while(next_size < required_size) {
        next_size *= 2;
    }
    *current_size = next_size;
    mem = realloc(mem, next_size);

    c->mem = mem;
    return mem;
}

void _msgpack_buffer_expand(msgpack_buffer_t* b, const char* data, size_t length, bool flush_to_io)
{
    if(flush_to_io && b->io != Qnil) {
        msgpack_buffer_flush(b);
        if(msgpack_buffer_writable_size(b) >= length) {
            /* data == NULL means ensure_writable */
            if(data != NULL) {
                size_t tail_avail = msgpack_buffer_writable_size(b);
                memcpy(b->tail.last, data, length);
                b->tail.last += tail_avail;
            }
            return;
        }
    }

    /* data == NULL means ensure_writable */
    if(data != NULL) {
        size_t tail_avail = msgpack_buffer_writable_size(b);
        memcpy(b->tail.last, data, tail_avail);
        b->tail.last += tail_avail;
        data += tail_avail;
        length -= tail_avail;
    }

    size_t capacity = b->tail.last - b->tail.first;

    /* can't realloc mapped chunk or rmem page */
    if(b->tail.mapped_string != NO_MAPPED_STRING
#ifndef DISABLE_RMEM
            || capacity <= MSGPACK_RMEM_PAGE_SIZE
#endif
            ) {
        /* allocate new chunk */
        _msgpack_buffer_add_new_chunk(b);

        char* mem = (char*)_msgpack_buffer_chunk_malloc(b, &b->tail, length, &capacity);
        //printf("tail malloc()ed capacity %lu\n", capacity);

        char* last = mem;
        if(data != NULL) {
            memcpy(mem, data, length);
            last += length;
        }

        /* rebuild tail chunk */
        b->tail.first = mem;
        b->tail.last = last;
        b->tail.mapped_string = NO_MAPPED_STRING;
        b->tail_buffer_end = mem + capacity;

        /* consider read_buffer */
        if(b->head == &b->tail) {
            b->read_buffer = b->tail.first;
        }

    } else {
        /* realloc malloc()ed chunk or NULL */
        size_t tail_filled = b->tail.last - b->tail.first;
        char* mem = (char*)_msgpack_buffer_chunk_realloc(b, &b->tail,
                b->tail.first, tail_filled+length, &capacity);
        //printf("tail realloc()ed capacity %lu filled=%lu\n", capacity, tail_filled);

        char* last = mem + tail_filled;
        if(data != NULL) {
            memcpy(last, data, length);
            last += length;
        }

        /* consider read_buffer */
        if(b->head == &b->tail) {
            size_t read_offset = b->read_buffer - b->head->first;
            //printf("relink read_buffer read_offset %lu\n", read_offset);
            b->read_buffer = mem + read_offset;
        }

        /* rebuild tail chunk */
        b->tail.first = mem;
        b->tail.last = last;
        b->tail_buffer_end = mem + capacity;
        //printf("alloced chunk size: %lu\n", msgpack_buffer_top_readable_size(b));
    }
}

static inline VALUE _msgpack_buffer_head_chunk_as_string(msgpack_buffer_t* b)
{
    size_t length = b->head->last - b->read_buffer;
    if(length == 0) {
        return rb_str_buf_new(0);
    }

    if(b->head->mapped_string != NO_MAPPED_STRING) {
        return _msgpack_buffer_refer_head_mapped_string(b, length);
    }

    return rb_str_new(b->read_buffer, length);
}

static inline VALUE _msgpack_buffer_chunk_as_string(msgpack_buffer_chunk_t* c)
{
    size_t chunk_size = c->last - c->first;
    if(chunk_size == 0) {
        return rb_str_buf_new(0);
    }

    if(c->mapped_string != NO_MAPPED_STRING) {
        return rb_str_dup(c->mapped_string);
    }

    return rb_str_new(c->first, chunk_size);
}

static VALUE
_msgpack_buffer_snappify_string(VALUE src)
{
    VALUE  dst;
    size_t  output_length;

    output_length = snappy::MaxCompressedLength(RSTRING_LEN(src));

    dst = rb_str_new(NULL, output_length);

    snappy::RawCompress(RSTRING_PTR(src), RSTRING_LEN(src), RSTRING_PTR(dst), &output_length);
    rb_str_resize(dst, output_length);

    return dst;
}

VALUE msgpack_buffer_all_as_string(msgpack_buffer_t* b)
{
    if(b->head == &b->tail) {
        return _msgpack_buffer_snappify_string(_msgpack_buffer_head_chunk_as_string(b));
    }

    size_t length = msgpack_buffer_all_readable_size(b);
    VALUE string = rb_str_new(NULL, length);
    char* buffer = RSTRING_PTR(string);

    size_t avail = msgpack_buffer_top_readable_size(b);
    memcpy(buffer, b->read_buffer, avail);
    buffer += avail;
    length -= avail;

    msgpack_buffer_chunk_t* c = b->head->next;

    while(true) {
        avail = c->last - c->first;
        memcpy(buffer, c->first, avail);

        if(length <= avail) {
            return _msgpack_buffer_snappify_string(string);
        }
        buffer += avail;
        length -= avail;

        c = c->next;
    }
}

VALUE msgpack_buffer_all_as_string_array(msgpack_buffer_t* b)
{
    if(b->head == &b->tail) {
        VALUE s = msgpack_buffer_all_as_string(b);
        VALUE ary = rb_ary_new();  /* TODO capacity = 1 */
        rb_ary_push(ary, s);
        return ary;
    }

    /* TODO optimize ary construction */
    VALUE ary = rb_ary_new();

    VALUE s = _msgpack_buffer_head_chunk_as_string(b);
    rb_ary_push(ary, s);

    msgpack_buffer_chunk_t* c = b->head->next;

    while(true) {
        s = _msgpack_buffer_chunk_as_string(c);
        rb_ary_push(ary, s);
        if(c == &b->tail) {
            return ary;
        }
        c = c->next;
    }

    return ary;
}

size_t msgpack_buffer_flush_to_io(msgpack_buffer_t* b, VALUE io, ID write_method, bool consume)
{
    if(msgpack_buffer_top_readable_size(b) == 0) {
        return 0;
    }

    VALUE s = _msgpack_buffer_head_chunk_as_string(b);
    rb_funcall(io, write_method, 1, s);
    size_t sz = RSTRING_LEN(s);

    if(consume) {
        while(_msgpack_buffer_shift_chunk(b)) {
            s = _msgpack_buffer_chunk_as_string(b->head);
            rb_funcall(io, write_method, 1, s);
            sz += RSTRING_LEN(s);
        }
        return sz;

    } else {
        if(b->head == &b->tail) {
            return sz;
        }
        msgpack_buffer_chunk_t* c = b->head->next;
        while(true) {
            s = _msgpack_buffer_chunk_as_string(c);
            rb_funcall(io, write_method, 1, s);
            sz += RSTRING_LEN(s);
            if(c == &b->tail) {
                return sz;
            }
            c = c->next;
        }
    }
}

size_t _msgpack_buffer_feed_from_io(msgpack_buffer_t* b)
{
    if(b->io_buffer == Qnil) {
        b->io_buffer = rb_funcall(b->io, b->io_partial_read_method, 1, LONG2FIX(b->io_buffer_size));
        if(b->io_buffer == Qnil) {
            rb_raise(rb_eEOFError, "IO reached end of file");
        }
        StringValue(b->io_buffer);
    } else {
        rb_funcall(b->io, b->io_partial_read_method, 2, LONG2FIX(b->io_buffer_size), b->io_buffer);
    }

    size_t len = RSTRING_LEN(b->io_buffer);
    if(len == 0) {
        rb_raise(rb_eEOFError, "IO reached end of file");
    }

    /* TODO zero-copy optimize? */
    msgpack_buffer_append_nonblock(b, RSTRING_PTR(b->io_buffer), len);

    return len;
}

size_t _msgpack_buffer_read_from_io_to_string(msgpack_buffer_t* b, VALUE string, size_t length)
{
    if(RSTRING_LEN(string) == 0) {
        /* direct read */
        rb_funcall(b->io, b->io_partial_read_method, 2, LONG2FIX(length), string);
        return RSTRING_LEN(string);
    }

    /* copy via io_buffer */
    if(b->io_buffer == Qnil) {
        b->io_buffer = rb_str_buf_new(0);
    }

    rb_funcall(b->io, b->io_partial_read_method, 2, LONG2FIX(length), b->io_buffer);
    size_t rl = RSTRING_LEN(b->io_buffer);

    rb_str_buf_cat(string, (const char*)RSTRING_PTR(b->io_buffer), rl);
    return rl;
}

size_t _msgpack_buffer_skip_from_io(msgpack_buffer_t* b, size_t length)
{
    if(b->io_buffer == Qnil) {
        b->io_buffer = rb_str_buf_new(0);
    }

    rb_funcall(b->io, b->io_partial_read_method, 2, LONG2FIX(length), b->io_buffer);
    return RSTRING_LEN(b->io_buffer);
}

