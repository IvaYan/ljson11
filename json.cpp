/*
 * JSON module interface.
*/

#include <lua/lua.hpp>
#include <string>
#include <cassert>
#include "json11/json11.hpp"


/// Convert an object on the top of the stack to JSON string
int json_tojson(lua_State* vm);
/// Parse a string on the top of the stack as JSON and create an object.
int json_fromjson(lua_State* vm);
/// Push a JSON value onto the stack (supports arrays & objects).
int json_push_value(lua_State* vm, const json11::Json& obj);
/**
 * Returns a Json object initialized with the value that is at the given stack index.
 * If something goes wrong, returns NULL and reports an error through the last argument.
 */
json11::Json json_tojson(lua_State* vm, int idx, std::string& err);
/// Convert an array to an object, convert integer array indices to string object keys.
void json_arr_to_obj(json11::Json::array& from, json11::Json::object& to);

extern "C" {

#ifdef _MSC_VER
    _declspec(dllexport)
#endif
    /// Module entry point that is called by Lua when it loads the module.
    int luaopen_json(lua_State* vm) {

        static luaL_Reg methods[] = {
            {"ToJson", json_tojson},
            {"FromJson", json_fromjson},
            {nullptr, nullptr}
        };

        luaL_newlib(vm, methods); // will push a table onto the stack
        return 1;// 1 value
    }
}


int json_tojson(lua_State* vm) {
    luaL_checktype(vm, 1, LUA_TTABLE); // one arg, a table

    do {
        std::string err;
        auto obj = json_tojson(vm, 1, err);
        if (!err.empty()) {
            lua_pushlstring(vm, err.c_str(), err.length());
            break;
        }

        auto dump = obj.dump();
        lua_pushlstring(vm, dump.c_str(), dump.length());
        return 1;
    } while(false);


    assert(lua_type(vm, -1) == LUA_TSTRING); // error expected
    return lua_error(vm);
}


int json_fromjson(lua_State* vm) {
    auto input = lua_tostring(vm, 1);

    do {
        std::string err;
        auto obj = json11::Json::parse(input, err, json11::JsonParse::COMMENTS); // allow comments
        if (!err.empty() && obj.is_null()) { // something went wrong
            lua_pushlstring(vm, err.c_str(), err.length());
            break;
        }

        /**
         * JSON does not support anything that is not supported by Lua (of data types).
         * As a result we are sure that there will be no errors.
         */
        return json_push_value(vm, obj);
    } while(false); // goto ^__^

    assert(lua_type(vm, -1) == LUA_TSTRING); // error expected
    return lua_error(vm);
}

int json_push_value(lua_State* vm, const json11::Json& obj) {

    /*
     * enum Type {
        NUL, NUMBER, BOOL, STRING, ARRAY, OBJECT
       };
     */
    switch (obj.type()) {
        case json11::Json::NUL:
            lua_pushnil(vm);
            return 1;
        case json11::Json::NUMBER:
            lua_pushnumber(vm, obj.number_value());
            return 1;
        case json11::Json::BOOL:
            lua_pushboolean(vm, obj.bool_value());
            return 1;
        case json11::Json::STRING: {
            auto &str = obj.string_value();
            lua_pushlstring(vm, str.c_str(), str.length());
            return 1;
        }
        default:
            break; // do nothing
    }
    if (obj.type() == json11::Json::ARRAY) {
        auto& arr = obj.array_items();
        lua_createtable(vm, static_cast<int>(arr.size()), 0); // reserve some memory
        auto idx = 1; // in Lua indices start at 1
        for (auto& item : arr) {
            json_push_value(vm, item);
            lua_rawseti(vm, -2, idx++);
        }
    } else {
        assert(obj.type() == json11::Json::OBJECT);

        auto &dict = obj.object_items();
        lua_createtable(vm, 0, static_cast<int>(dict.size())); // reserve memory for elements

        for (auto& item: dict) {
            lua_pushlstring(vm, item.first.c_str(), item.first.length()); // key
            json_push_value(vm, item.second); // push the value
            lua_rawset(vm, -3);
        }
    }

    return 1; // one value is on the top of the stack
}

json11::Json json_tojson(lua_State* vm, int idx, std::string& err) {
    err.clear();
    auto type = lua_type(vm, idx);

    switch (type) {
        case LUA_TNIL:
            return nullptr;
        case LUA_TBOOLEAN:
            return json11::Json(static_cast<bool>(lua_toboolean(vm, idx)));
        case LUA_TNUMBER:
            return json11::Json(lua_tonumber(vm, idx));
        case LUA_TSTRING: {
            size_t len;
            auto str = lua_tolstring(vm, idx, &len);
            return json11::Json(std::string(str, len));
        }
            break;
        case LUA_TLIGHTUSERDATA:
        case LUA_TFUNCTION:
        case LUA_TUSERDATA:
        case LUA_TTHREAD:
            err = "Unsupported data type. Only nil, number, boolean, string and table are supported.";
            return nullptr;
        default: // do nothing here
            break;
    }

    assert(type == LUA_TTABLE);
    json11::Json::object obj;
    json11::Json::array arr;
    auto use_obj = false; // whether to use object instread of array

    auto top = lua_gettop(vm);

    lua_pushnil(vm);
    if (idx < 0)
        idx -= 1; // we pushed one value, decrement index

    while (lua_next(vm, idx)) {
        /* 'key' is at index -2, 'value' is at index -1 */
        auto key_type = lua_type(vm, -2);
        if (key_type != LUA_TSTRING && !lua_isinteger(vm, -2)) {
            err = "Invalid key type: only string and integers allowed.";
            lua_settop(vm, top);
            return nullptr;
        }

        if (!use_obj) {
            // the key is a string, so convert to an object if we are using an array
            if (key_type == LUA_TSTRING) {
                json_arr_to_obj(arr, obj);
                use_obj = true;
            } else if (key_type == LUA_TNUMBER) {
                assert(lua_isinteger(vm, -2));
                auto key = lua_tointeger(vm, -2);
                // key must be > 1 and equal to the size of the array we work with +1
                // (because in Lua indices start at 1)
                if (key < 1 || (key - 1) != arr.size()) {
                    json_arr_to_obj(arr, obj);
                    use_obj = true;
                }
            }
        }

        auto value = json_tojson(vm, -1, err);
        if (!err.empty()) {
            lua_settop(vm, top);
            return nullptr;
        }

        if (use_obj) {
            // it is almost always possible to convert something to a string
            lua_pushvalue(vm, -2); // clone key because tostring modifies obj type
            auto key = lua_tostring(vm, -1);
            assert(key);
            obj[key] = std::move(value);
            lua_pop(vm, 1);
        } else {
            assert(lua_isinteger(vm, -2));
            arr.push_back(std::move(value));
        }

        lua_pop(vm, 1); // pop value, leave key for the next call
    } // while (lua_next(vm, idx)) {

    if (use_obj)
        return obj;
    return arr;
}

void json_arr_to_obj(json11::Json::array& from, json11::Json::object& to) {
    to.clear();

    for (auto i = 0u; i < from.size(); ++i) {
        auto key = std::to_string(i); // although in Lua indices start at 1, in JSON they start at 0
        to[key] = std::move(from[i]);
    }
    from.clear();
}
