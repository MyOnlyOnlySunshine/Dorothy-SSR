/* tolua: funcitons to convert to C types
** Support code for Lua bindings.
** Written by Waldemar Celes, modified by Jin Li
** TeCGraf/PUC-Rio
** Apr 2003, Apr 2014
** $Id: $
*/

/* This code is free software; you can redistribute it and/or modify it.
** The software provided hereunder is on an "as is" basis, and
** the author has no obligation to provide maintenance, support, updates,
** enhancements, or modifications.
*/

#include "Const/oHeader.h"
#include "tolua++.h"

NS_DOROTHY_BEGIN

lua_Number tolua_tonumber(lua_State* L, int narg, lua_Number def)
{
	return lua_gettop(L) < abs(narg) ? def : lua_tonumber(L, narg);
}

const char* tolua_tostring(lua_State* L, int narg, const char* def)
{
	return lua_gettop(L) < abs(narg) ? def : lua_tostring(L, narg);
}

void* tolua_touserdata(lua_State* L, int narg, void* def)
{
	/* return lua_gettop(L)<abs(narg) ? def : lua_touserdata(L,narg); */
	if(lua_gettop(L) < abs(narg))
	{
		return def;
	}
	if(lua_islightuserdata(L, narg))
	{
		return lua_touserdata(L, narg);
	}
	return tolua_tousertype(L, narg, def);
}

void* tolua_tousertype(lua_State* L, int narg, void* def)
{
	if(lua_gettop(L) < abs(narg))
	{
		return def;
	}
	else
	{
		void* u = lua_touserdata(L, narg);
		/* nil represents NULL */
		return (u == NULL) ? NULL : *((void**)u);
	}
}

int tolua_tovalue(lua_State* L, int narg, int def)
{
	return lua_gettop(L) < abs(narg) ? def : narg;
}

bool tolua_toboolean(lua_State* L, int narg, int def)
{
	return (lua_gettop(L) < abs(narg) ? def : lua_toboolean(L, narg)) != 0;
}

lua_Number tolua_tofieldnumber(lua_State* L, int lo, int index, lua_Number def)
{
	lua_Number v;
	lua_pushnumber(L, index);
	lua_gettable(L, lo);
	v = lua_isnil(L, -1) ? def : lua_tonumber(L, -1);
	lua_pop(L, 1);
	return v;
}

const char* tolua_tofieldstring(lua_State* L, int lo, int index, const char* def)
{
	const char* v;
	lua_pushnumber(L, index);
	lua_gettable(L, lo);
	v = lua_isnil(L, -1) ? def : lua_tostring(L, -1);
	lua_pop(L, 1);
	return v;
}

void* tolua_tofielduserdata(lua_State* L, int lo, int index, void* def)
{
	void* v;
	lua_pushnumber(L, index);
	lua_gettable(L, lo);
	v = lua_isnil(L, -1) ? def : lua_touserdata(L, -1);
	lua_pop(L, 1);
	return v;
}

void* tolua_tofieldusertype(lua_State* L, int lo, int index, void* def)
{
	void* v;
	lua_pushnumber(L, index);
	lua_gettable(L, lo);
	v = lua_isnil(L, -1) ? def :(*(void **)(lua_touserdata(L, -1))); /* lua_unboxpointer(L,-1); */
	lua_pop(L, 1);
	return v;
}

int tolua_tofieldvalue(lua_State* L, int lo, int index, int def)
{
	int v;
	lua_pushnumber(L, index);
	lua_gettable(L, lo);
	v = lua_isnil(L, -1) ? def : lo;
	lua_pop(L, 1);
	return v;
}

int tolua_getfieldboolean(lua_State* L, int lo, int index, int def)
{
	int v;
	lua_pushnumber(L, index);
	lua_gettable(L, lo);
	v = lua_isnil(L, -1) ? 0 : lua_toboolean(L, -1);
	lua_pop(L, 1);
	return v;
}

NS_DOROTHY_END
