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
#ifndef MSGPACK_RUBY_BUFFER_CLASS_H__
#define MSGPACK_RUBY_BUFFER_CLASS_H__

#include "buffer.hh"

extern VALUE cMessagePack_Buffer;

extern "C" void MessagePack_Buffer_module_init(VALUE mMessagePack);

extern "C" VALUE MessagePack_Buffer_wrap(msgpack_buffer_t* b, VALUE owner);

void MessagePack_Buffer_initialize(msgpack_buffer_t* b, VALUE io, VALUE options);

#endif

