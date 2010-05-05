#! /usr/bin/env escript

main([]) ->
    code:add_pathz("test"),
    code:add_pathz("ebin"),

    etap:plan(unknown),
    ObjId1 = test_basic(),
    erlang:garbage_collect(),
    etap:is(abacus_nifs:drain(), [0], "Drain returned our object id."),
    etap:is(abacus_nifs:drain(), [], "Re-drain returned nothing."),
    etap:is(abacus_nifs:freed(), {ok, 1}, "The object was freed."),

    test_delayed_free(),
    erlang:garbage_collect(),
    etap:is(abacus_nifs:drain(), [], "Delayed free didn't re-add obj_id"),
    etap:is(abacus_nifs:freed(), {ok, 2}, "The object was freed."),

    test_retry(),
    erlang:garbage_collect(),
    etap:is(abacus_nifs:drain(), [], "Delayed free didn't re-add ObjId"),
    etap:is(abacus_nifs:freed(), {ok, 3}, "Freed another object."),

    test_no_incref(),
    etap:is(abacus_nifs:freed(), {ok, 4}, "Object freed but not drained."),

    etap:end_tests().

test_basic() ->
    {ok, RefCnt, ObjId} = abacus_nifs:mkref(),
    etap:is(abacus_nifs:refcnt(RefCnt), {ok, 1}, "Count initialized as 1"),
    etap:is(abacus_nifs:obj_id(RefCnt), {ok, ObjId}, "Object id was set."),
    {ok, NewCnt} = abacus_nifs:incref(RefCnt),
    etap:is(abacus_nifs:refcnt(NewCnt), {ok, 2}, "Incref incremented count."),
    etap:is(abacus_nifs:obj_id(NewCnt), {ok, ObjId}, "Object id was set"),
    etap:is(abacus_nifs:refcnt(RefCnt), {ok, 2}, "Count is shared."),
    ObjId.

test_delayed_free() ->
    {RefCnt, ObjId} = test_delayed_free_sub(),
    erlang:garbage_collect(),
    etap:is(abacus_nifs:refcnt(RefCnt), {ok, 1}, "Count was decremented."),
    etap:is(abacus_nifs:drain(), [ObjId], "Drain returns on count == 1"),
    etap:is(abacus_nifs:drain(), [], "Re-drain is empty."). 

test_delayed_free_sub() ->
    {ok, RefCnt, ObjId} = abacus_nifs:mkref(),
    {ok, NewCnt} = abacus_nifs:incref(RefCnt),
    etap:is(abacus_nifs:refcnt(NewCnt), {ok, 2}, "Count is good after setup."),
    {RefCnt, ObjId}.

test_retry() ->
    {RefCnt, ObjId} = test_retry_sub(),
    erlang:garbage_collect(),
    etap:is(abacus_nifs:refcnt(RefCnt), {ok, 1}, "Count was decremented."),
    etap:is(abacus_nifs:incref(RefCnt), retry, "Retry, ObjId is dead."),
    etap:is(abacus_nifs:drain(), [ObjId], "Drain contained ObjId"),
    etap:is(abacus_nifs:drain(), [], "Re-drain is empty."),
    etap:is(abacus_nifs:incref(RefCnt), retry, "Retry, ObjId still dead.").

test_retry_sub() ->
    {ok, RefCnt, ObjId} = abacus_nifs:mkref(),
    {ok, NewCnt} = abacus_nifs:incref(RefCnt),
    etap:is(abacus_nifs:refcnt(NewCnt), {ok, 2}, "Count is good after setup."),
    {RefCnt, ObjId}.

test_no_incref() ->
    test_no_incref_sub(),
    erlang:garbage_collect(),
    etap:is(abacus_nifs:drain(), [], "No incref means not drained.").
    
test_no_incref_sub() ->
    {ok, RefCnt, ObjId} = abacus_nifs:mkref(),
    etap:is(abacus_nifs:refcnt(RefCnt), {ok, 1}, "Count is 1 after init.").
