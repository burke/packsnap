# Packsnap

MessagePack is great at serializing data quickly.

Snappy is great at compressing data quickly.

Packsnap serializes datastructures using MessagePack and Snappy.

# Examples

    Packsnap.pack("a simple string")
    => "\x10<\xAFa simple string"

    Packsnap.pack(["an array", 3])
    => "\v(\x92\xA8an array\x03"

    Packsnap.pack("long key" * 15)
    => "{(\xDA\x00xlong key\xFE\b\x00\xBE\b\x00"

    # And a totally useless benchmark (Macbook Air 11")
    Benchmark.realtime { 1_000_000.times { Packsnap.pack("value") } }
    => 1.654603

# Copyright

MessagePack code copyright 2012 FURUHASHI Sadayuki

Additions copyright Burke Libbey

License: Apache License, Version 2.0

