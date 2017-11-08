Thin wrapper around Dropbox' json11 library that enables Lua to
serialize/deserialize data to/from JSON.

Read [this article](https://medium.com/@243cd1d8/json11-bindings-for-lua-behind-the-scenes-1f36c19da9cb) to know more about internals.

## Compiling

Use a build system of your choice. Just create
a new project or a makefile that will produce
a shared library (dll/so/dylib) and add `json.cpp`
and `json11.cpp` files.

## Usage
When the module is loaded, its entry point creates and returns
a table with two fields: `ToJson` and `FromJson`. The first function
returns a string that is a JSON representation of the argument.
The second function performs the reversed operation: given a JSON
string returns a Lua value that was encoded as JSON.

### Supported data types
JSON supports only NULL, strings, boolean values, arrays and objects
(dictionaries), as a result `ToJson` function accepts values of these
types only. It supports tables that are arrays and tables that are
dictionaries, and chooses the proper JSON-type for them
(see 'Table Handling' section for details).
Functions, userdata, light userdata, threads are not supported and
could not be converted to JSON.

### Example
```lua
local json = require 'json'

local data = {
    menu={
        id='file',
        value='File',
        popup={
                menuitem={
                    {value='New', onclick='CreateNewDoc()'},
                    {value='Open', onclick='OpenDoc()'},
                    {value='Close', onclick='CloseDoc()'},
                }
            }
        }
}

local json_str = json.ToJson(data)
print(json_str)
local object = json.FromJson(json_str)
``` 

## Table handling
When it comes to JSON-serialization, it is a bit
tricky to handle tables properly. In Lua there is no
difference between arrays and dictionaries, these two
data structures are implemented with tables.
As a result, given an arbitrary table it might be tricky to
determine whether it is an array or a dictionary.

In order to support both arrays and dictionaries we try to
detect the real type of a table. We start iterating table
and handle this table as an array, but when we find a key that
is not an integer, or there is a gap between two sequential keys
(like 1 and 3) we change out minds and treat the table as a
dictionary. We move all values from an array to the new dictionary
and assign them appropriate keys. It is always possible to convert
an array to a dictionary, but in general it is not possible to
convert a dictionary to an array.
