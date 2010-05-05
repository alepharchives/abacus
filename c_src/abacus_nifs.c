#include <stdio.h>
#include "erl_nif.h"

static ErlNifResourceType* abacus_shared_ptr;
static ERL_NIF_TERM ATOM_OK;
static ERL_NIF_TERM ATOM_ERROR;
static ERL_NIF_TERM ATOM_RETRY;

typedef unsigned long ulong;

typedef struct _abacus_ref_t abacus_ref_t;
struct _abacus_ref_t
{
    ErlNifMutex*    lock;
    ulong           count;
    ulong           obj_id;
    char            dead;
    abacus_ref_t*   next;
};

typedef struct
{
    abacus_ref_t*   obj;
} abacus_handle;

typedef struct
{
    ErlNifMutex*    lock;
    abacus_ref_t*   head;
    abacus_ref_t*   tail;
    ulong           next_id;
    ulong           freed;
} abacus_priv_t;


ERL_NIF_TERM
mkref(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    ERL_NIF_TERM ret;
    abacus_handle* handle;
    abacus_priv_t* priv = (abacus_priv_t*) enif_priv_data(env);
    ulong obj_id;
    
    if(argc != 0) return enif_make_badarg(env);
    
    handle = (abacus_handle*) enif_alloc_resource(
        env,
        abacus_shared_ptr,
        sizeof(abacus_handle)
    );

    enif_mutex_lock(priv->lock);
    obj_id = priv->next_id++;
    enif_mutex_unlock(priv->lock);
    
    handle->obj = (abacus_ref_t*) enif_alloc(env, sizeof(abacus_ref_t));
    handle->obj->lock = enif_mutex_create("abacus_shared_ptr_lock");
    handle->obj->count = 1;
    handle->obj->obj_id = obj_id;
    handle->obj->dead = 0;
    handle->obj->next = NULL;
    
    ret = enif_make_resource(env, handle);
    enif_release_resource(env, handle);
    return enif_make_tuple3(env, ATOM_OK, ret, enif_make_ulong(env, obj_id));
}

ERL_NIF_TERM
incref(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    ERL_NIF_TERM ret;
    abacus_handle* handle;
    abacus_handle* newhandle;
    
    if(argc != 1) return enif_make_badarg(env);
    if(!enif_get_resource(env, argv[0], abacus_shared_ptr, (void**) &handle))
    {
        return enif_make_badarg(env);
    }
    
    enif_mutex_lock(handle->obj->lock);
    if(handle->obj->dead)
    {
        ret = ATOM_RETRY;
    }
    else
    {
        newhandle = (abacus_handle*) enif_alloc_resource(
                env,
                abacus_shared_ptr,
                sizeof(abacus_handle)
            );
        newhandle->obj = handle->obj;
        newhandle->obj->count += 1;
        ret = enif_make_resource(env, newhandle);
        enif_release_resource(env, newhandle);
        ret = enif_make_tuple2(env, ATOM_OK, ret);
    }
    enif_mutex_unlock(handle->obj->lock);
    return ret;
}

ERL_NIF_TERM
obj_id(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    ulong obj_id;
    abacus_handle* handle;
    
    if(argc != 1) return enif_make_badarg(env);
    if(!enif_get_resource(env, argv[0], abacus_shared_ptr, (void**) &handle))
    {
        return enif_make_badarg(env);
    }
    
    enif_mutex_lock(handle->obj->lock);
    obj_id = handle->obj->obj_id;
    enif_mutex_unlock(handle->obj->lock);
    
    return enif_make_tuple2(env, ATOM_OK, enif_make_ulong(env, obj_id));
}

ERL_NIF_TERM
refcnt(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    ulong count;
    abacus_handle* handle;
    
    if(argc != 1) return enif_make_badarg(env);
    if(!enif_get_resource(env, argv[0], abacus_shared_ptr, (void**) &handle))
    {
        return enif_make_badarg(env);
    }
    
    enif_mutex_lock(handle->obj->lock);
    count = handle->obj->count;
    enif_mutex_unlock(handle->obj->lock);
    
    return enif_make_tuple2(env, ATOM_OK, enif_make_ulong(env, count));
}

ERL_NIF_TERM
drain(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    ERL_NIF_TERM ret;
    ERL_NIF_TERM obj_id;
    abacus_priv_t* priv;
    abacus_ref_t* ref;

    if(argc != 0) return enif_make_badarg(env);
    
    priv = (abacus_priv_t*) enif_priv_data(env);
    
    enif_mutex_lock(priv->lock);
    ref = priv->head;
    priv->head = NULL;
    priv->tail = NULL;
    enif_mutex_unlock(priv->lock);
    
    ret = enif_make_list(env, 0);
    
    while(ref != NULL)
    {
        obj_id = enif_make_ulong(env, ref->obj_id);
        ret = enif_make_list_cell(env, obj_id, ret);
        ref = ref->next;
    }
    
    return ret;
}

ERL_NIF_TERM
freed(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    abacus_priv_t* priv = (abacus_priv_t*) enif_priv_data(env);
    ulong freed;

    if(argc != 0) return enif_make_badarg(env);
    
    enif_mutex_lock(priv->lock);
    freed = priv->freed;
    enif_mutex_unlock(priv->lock);

    return enif_make_tuple2(env, ATOM_OK, enif_make_ulong(env, freed));
}

void
free_abacus_shared_ptr(ErlNifEnv* env, void* data)
{
    abacus_priv_t* priv = (abacus_priv_t*) enif_priv_data(env);
    abacus_handle* handle = (abacus_handle*) data;
    char dead = 0;

    enif_mutex_lock(handle->obj->lock);
    handle->obj->count -= 1;
    
    // If its time to kill this object, add it to
    // the global linked list for abacus_server to
    // collect.
    if(handle->obj->count == 1)
    {
        handle->obj->dead = 1;
        enif_mutex_lock(priv->lock);
        if(priv->head == NULL)
        {
            priv->head = handle->obj;
            priv->tail = handle->obj;
        }
        else
        {
            priv->tail->next = handle->obj;
            priv->tail = handle->obj;
        }        
        enif_mutex_unlock(priv->lock);
    }
    else if(handle->obj->count == 0)
    {
        dead = 1;
    }

    enif_mutex_unlock(handle->obj->lock);
    
    if(dead) {
        enif_mutex_destroy(handle->obj->lock);
        enif_free(env, handle->obj);

        enif_mutex_lock(priv->lock);
        priv->freed++;
        enif_mutex_unlock(priv->lock);
    }    
}

static ErlNifFunc nif_funcs[] =
{
    {"mkref", 0, mkref},
    {"incref", 1, incref},
    {"obj_id", 1, obj_id},
    {"refcnt", 1, refcnt},
    {"drain", 0, drain},
    {"freed", 0, freed}
};

int
on_load(ErlNifEnv* env, void** priv_data, ERL_NIF_TERM load_info)
{
    abacus_priv_t* priv;
    
    abacus_shared_ptr = enif_open_resource_type(
            env,
            "abacus_shared_ptr",
            free_abacus_shared_ptr,
            ERL_NIF_RT_CREATE | ERL_NIF_RT_TAKEOVER,
            0
        );

    ATOM_OK = enif_make_atom(env, "ok");
    ATOM_ERROR = enif_make_atom(env, "error");
    ATOM_RETRY = enif_make_atom(env, "retry");

    priv = enif_alloc(env, sizeof(abacus_priv_t));
    priv->lock = enif_mutex_create("abacus_global_list_lock");
    priv->head = NULL;
    priv->tail = NULL;
    priv->next_id = 0;
    *priv_data = priv;

    return 0;
}

ERL_NIF_INIT(abacus_nifs, nif_funcs, &on_load, NULL, NULL, NULL);
