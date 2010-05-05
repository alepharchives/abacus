Abacus
======

An initial attempt at reference counting in Erlang. Based on NIF resource
types that provide a triggered reference counting mechanism.

Currently the NIF side appears to be working. Lots left to do on the Erlang
side in handling the few conditions that are awkward due to NIF limitations.
