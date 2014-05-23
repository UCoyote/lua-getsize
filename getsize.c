/* lua-getsize
   Author: (C) 2009 Matthew Wild
   License: MIT/X11 license
   Description: Adds a debug.getsize() function which
                returns the size in bytes of a Lua object
*/

#include <stdio.h>

#include <lua.h>
#include <lauxlib.h>
#include <lstate.h>
#include <lobject.h>
#include <lfunc.h>
#include <lstring.h>


#if LUA_VERSION_NUM == 501
#define ARG(L, n) (((L)->base+(n))-1)
#elif LUA_VERSION_NUM == 502 || \
      LUA_VERSION_NUM == 503
#define ARG(L, n) ((L)->ci->func+(n))
#endif


static size_t sizeProto(Proto const *p)
{
  return sizeof(Proto) + sizeof(Instruction) * p->sizecode +
                         sizeof(Proto*) * p->sizep +
                         sizeof(TValue) * p->sizek +
                         sizeof(int) * p->sizelineinfo +
                         sizeof(LocVar) * p->sizelocvars +
#if LUA_VERSION_NUM == 501
                         sizeof(TString*) * p->sizeupvalues;
#elif LUA_VERSION_NUM == 502 || \
      LUA_VERSION_NUM == 503
                         sizeof(Upvaldesc) * p->sizeupvalues;
#endif
}


static int debug_getsize(lua_State *L)
{
  Node const *dummynode = lua_touserdata(L, lua_upvalueindex(1));
  TValue *o = ARG(L, 1);
  size_t olen = 0;
  char const *options = luaL_optlstring(L, 2, "", &olen);
  int count_upvalues = 1;
  int count_protos = 0;
  size_t i = 0;
  for (i = 0; i < olen; ++i) {
    switch (options[i]) {
      case 'p': count_protos = 1; break;
      case 'P': count_protos = 0; break;
      case 'u': count_upvalues = 1; break;
      case 'U': count_upvalues = 0; break;
      default:
        luaL_error(L, "unkown option for 'getsize': %c", (int)options[i]);
        break;
    }
  }
  switch (ttype(o)) {
    case LUA_TTABLE: {
      Table *h = hvalue(o);
      lua_pushinteger(L, sizeof(Table) + sizeof(TValue) * h->sizearray +
                         sizeof(Node) * (h->node == dummynode ? 0 : sizenode(h)));
      break;
    }
#if LUA_VERSION_NUM == 501
    case LUA_TFUNCTION: {
      Closure *cl = clvalue(o);
      if (cl->c.isC)
        lua_pushinteger(L, sizeCclosure(cl->c.nupvalues));
      else
        lua_pushinteger(L, sizeLclosure(cl->l.nupvalues) +
                           (count_upvalues ? cl->l.nupvalues * sizeof(UpVal) : 0) +
                           (count_protos ? sizeProto(cl->l.p) : 0));
      break;
    }
#elif LUA_VERSION_NUM == 502 || \
      LUA_VERSION_NUM == 503
    case LUA_TLCL: { /* Lua closure */
      Closure *cl = clvalue(o);
      lua_pushinteger(L, sizeLclosure(cl->l.nupvalues) +
                         (count_upvalues ? cl->l.nupvalues * sizeof(UpVal) : 0) +
                         (count_protos ? sizeProto(cl->l.p) : 0));
      break;
    }
    case LUA_TLCF: { /* light C function */
      lua_pushinteger(L, sizeof(lua_CFunction));
      break;
    }
    case LUA_TCCL: { /* C closure */
      Closure *cl = clvalue(o);
      lua_pushinteger(L, sizeCclosure(cl->c.nupvalues));
      break;
    }
#endif
#if LUA_VERSION_NUM == 501
    case LUA_TTHREAD: {
      lua_State *th = thvalue(o);
      lua_pushinteger(L, sizeof(lua_State) + sizeof(TValue) * th->stacksize +
                         sizeof(CallInfo) * th->size_ci);
      break;
    }
#elif LUA_VERSION_NUM == 502 || \
      LUA_VERSION_NUM == 503
    case LUA_TTHREAD: {
      lua_State *th = thvalue(o);
      CallInfo *ci = th->base_ci.next;
      size_t cisize = 0;
      for (; ci != NULL; ci = ci->next)
        cisize += sizeof(CallInfo);
      lua_pushinteger(L, sizeof(lua_State) + sizeof(TValue) * th->stacksize +
                         cisize);
      break;
    }
#endif
    case LUA_TUSERDATA: {
      lua_pushinteger(L, sizeudata(uvalue(o)));
      break;
    }
    case LUA_TLIGHTUSERDATA: {
      lua_pushinteger(L, sizeof(void*));
      break;
    }
#if LUA_VERSION_NUM == 502 || \
    LUA_VERSION_NUM == 503
    case LUA_TLNGSTR: /* fall through */
#endif
    case LUA_TSTRING: {
      lua_pushinteger(L, sizestring(tsvalue(o)));
      break;
    }
#if LUA_VERSION_NUM == 503
    case LUA_TNUMINT: {
      lua_pushinteger(L, sizeof(lua_Integer));
      break;
    }
#endif
    case LUA_TNUMBER: {
      lua_pushinteger(L, sizeof(lua_Number));
      break;
    }
    case LUA_TBOOLEAN: {
      lua_pushinteger(L, sizeof(int));
      break;
    }
    case LUA_TNIL: {
      lua_pushinteger(L, 0);
      break;
    }
    default: return 0;
  }
  return 1;
}

int luaopen_getsize(lua_State* L)
{
  lua_settop(L, 0);
  lua_getglobal(L, "debug");
  lua_createtable(L, 0, 0); /* to get dummynode pointer */
  lua_pushlightuserdata(L, hvalue(ARG(L,2))->node);
  lua_pushcclosure(L, debug_getsize, 1);
  lua_replace(L, -2); /* remove dummy table */
  if (lua_type(L, 1) == LUA_TTABLE) {
    lua_pushvalue(L, -1);
    lua_setfield(L, 1, "getsize");
  }
  return 1;
}

