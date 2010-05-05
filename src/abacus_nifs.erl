-module(abacus_nifs).
-export([mkref/0, incref/1, obj_id/1, refcnt/1, drain/0, freed/0]).
-on_load(init/0).

init() ->
    SoName = case code:priv_dir(json) of
        {error, bad_name} ->
            case filelib:is_file(filename:join(["..", priv, abacus_nifs])) of
                true ->
                    filename:join(["..", priv, abacus_nifs]);
                _ ->
                    filename:join([priv, abacus_nifs])
            end;
        Dir ->
            filename:join(Dir, abacus_nifs)
    end,
    erlang:load_nif(SoName, 0).

mkref() ->
    not_loaded(?LINE).

incref(_Obj) ->
    not_loaded(?LINE).

obj_id(_Obj) ->
    not_loaded(?LINE).

refcnt(_Obj) ->
    not_loaded(?LINE).

drain() ->
    not_loaded(?LINE).
    
freed() ->
    not_loaded(?LINE).

not_loaded(Line) ->
    exit({abacus_not_loaded, module, ?MODULE, line, Line}).
