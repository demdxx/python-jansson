#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <Python.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <jansson.h>

#if HAVE_LOCALE_H
#include <locale.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef JSON_REJECT_DUPLICATES
    #define JSON_REJECT_DUPLICATES 0x1
#endif

#ifndef JSON_DISABLE_EOF_CHECK
    #define JSON_DISABLE_EOF_CHECK 0x2
#endif

#ifndef JSON_DECODE_ANY
    #define JSON_DECODE_ANY        0x4
#endif

#ifndef json_object_foreach
    #define json_object_foreach(object, key, value) \
        void* __iterator = json_object_iter(object); \
        for(key = json_object_iter_key(__iterator); \
            key && (value = json_object_iter_value(__iterator)); \
            (__iterator = json_object_iter_next(object, __iterator)), \
            (key = json_object_iter_key(__iterator)))
#endif

#define l_isspace(c) ((c) == ' ' || (c) == '\n' || (c) == '\r' || (c) == '\t')

static json_t * pyjansson_dict2json(PyObject *data);
static json_t * pyjansson_list2json(PyObject *data);
static json_t * pyjansson_tuple2json(PyObject *data);

static PyObject *JSON_Error;
static PyObject *JSON_EncodeError;
static PyObject *JSON_DecodeError;


/* Return a pointer to the first non-whitespace character of str.
   Modifies str so that all trailing whitespace characters are
   replaced by '\0'. */
static char *strip(char *str) {
    size_t length;
    char *result = str;
    while(*result && l_isspace(*result))
        result++;

    length = strlen(result);
    if(length == 0)
        return result;

    while(l_isspace(result[length - 1]))
        result[--length] = '\0';

    return result;
}

/**
 * Convert JSON object to PyObject
 * @param JSONObject
 * @return PyObject
 */
static PyObject* convert(json_t *json) {
    PyObject* object = NULL;
    json_t* val;
    int result = 0;

    if (json_is_object(json)) {
        object = PyDict_New();

        char* key;
        json_object_foreach(json, key, val) {
            if (json_is_object(val) || json_is_array(val)) {
                result = PyDict_SetItemString(object, key, convert(val));
            }
            else if(json_is_string(val)) {
                const char *str = json_string_value(val);
                result = PyDict_SetItemString(object, key,
                    PyString_DecodeEscape(str, str ? strlen(str) : 0, NULL, 0, NULL));
            }
            else if(json_is_boolean(val)) {
                result = PyDict_SetItemString(object, key,
                    json_is_true(val) ? Py_True : Py_False);
            }
            else if(json_is_integer(val)) {
                result = PyDict_SetItemString(object, key,
                    PyInt_FromLong(json_integer_value(val)));
            }
            else if(json_is_real(val)) {
                result = PyDict_SetItemString(object, key,
                    PyFloat_FromDouble(json_real_value(val)));
            }
            else {
                result = PyDict_SetItemString(object, key, Py_None);
            }
            if (result == -1)
                goto failure;
        }
    }
    else if (json_is_array(json)) {
        object = PyList_New(0);
    
        size_t size = json_array_size(json);
        size_t i=0;
        for (;i<size;i++) {
            val = json_array_get(json, i);

            if (json_is_object(val) || json_is_array(val)) {
                result = PyList_Append(object, convert(val));
            }
            else if(json_is_string(val)) {
                const char *str = json_string_value(val);
                //result = PyList_Append(object,
                //    PyUnicode_DecodeUnicodeEscape(str, str ? strlen(str) : 0, NULL));
                result = PyList_Append(object,
                    PyString_DecodeEscape(str, str ? strlen(str) : 0, NULL, 0, NULL));
            }
            else if(json_is_boolean(val)) {
                result = PyList_Append(object,
                    json_is_true(val) ? Py_True : Py_False);
            }
            else if(json_is_integer(val)) {
                result = PyList_Append(object,
                    PyInt_FromLong(json_integer_value(val)));
            }
            else if(json_is_real(val)) {
                result = PyList_Append(object,
                    PyFloat_FromDouble(json_real_value(val)));
            }
            else {
                result = PyList_Append(object, Py_None);
            }
        }
    }
    else {
        goto failure;
    }
    return object;

failure:
    Py_DECREF(object);
    return NULL;
}

/**
 * Parse JSON string and return dict
 * @param string
 * @return dict
 */
static PyObject * pyjansson_parse(PyObject *self, PyObject *args) {
    char *s_json;
    int flags = JSON_DECODE_ANY;
    if (!PyArg_ParseTuple(args, "s|i", &s_json, &flags)) {
        PyErr_SetString(PyExc_ValueError, "Invalid arguments");
        return NULL;
    }

    PyObject *object;
    json_t *json;
    json_error_t error;
    json = json_loads(strip(s_json), flags, &error);
    if (NULL==json) {
        // Error...
        object = NULL;
        PyErr_Format(JSON_DecodeError,
            "JSON string parse error: line[%d] column[%d] position[%d]\n%s",
                error.line, error.column, error.position, error.text);
    }
    else {
        object = convert(json);
        json_delete(json);
    }

    return object;
}

/**
 * Convert PyDict to JSONObject
 * @param PyDict
 * @return JSONObject
 */
static json_t * pyjansson_dict2json(PyObject *data) {
    PyObject *keys = PyDict_Keys(data);
    PyObject *key, *val;
    char *key_s;
    size_t size = PyList_Size(keys);
    size_t i;
    
    json_t *json = json_object();
    int result;

    for (i=0;i<size;i++) {
        key = PyList_GetItem(keys, i);
        val = PyDict_GetItem(data, key);
        key_s = PyString_AsString(key);
        
        if (PyDict_Check(val)) {
            result = json_object_set(json, key_s, pyjansson_dict2json(val));
        }
        else if (PyList_Check(val)) {
            result = json_object_set(json, key_s, pyjansson_list2json(val));
        }
        else if (PyTuple_Check(val)) {
            result = json_object_set(json, key_s, pyjansson_tuple2json(val));
        }
        else if (PyString_Check(val)) {
            result = json_object_set(json, key_s, json_string(PyString_AsString(val)));
        }
        else if (PyInt_Check(val)) {
            result = json_object_set(json, key_s, json_integer(PyInt_AsLong(val)));
        }
        else if (PyLong_Check(val)) {
            result = json_object_set(json, key_s, json_integer(PyLong_AsLong(val)));
        }
        else if (PyFloat_Check(val)) {
            result = json_object_set(json, key_s, json_real(PyFloat_AsDouble(val)));
        }
        else if (PyBool_Check(val)) {
            if (Py_False==val) {
                result = json_object_set(json, key_s, json_false());
            }
            else {
                result = json_object_set(json, key_s, json_true());
            }
        }
        else { // Py_None
            result = json_object_set(json, key_s, json_null());
        }
        if (-1 == result)
            goto failure;
    }
    return json;

failure:
    json_delete(json);
    return NULL;
}

/**
 * Convert PyList to JSONArray
 * @param PyList
 * @return JSONArray
 */
static json_t * pyjansson_list2json(PyObject *data) {
    PyObject *val;
    size_t size = PyList_Size(data);
    size_t i;
    
    json_t *json = json_array();
    int result;

    for (i=0;i<size;i++) {
        val = PyList_GetItem(data, i);

        if (PyDict_Check(val)) {
            result = json_array_append_new(json, pyjansson_dict2json(val));
        }
        else if (PyList_Check(val)) {
            result = json_array_append_new(json, pyjansson_list2json(val));
        }
        else if (PyTuple_Check(val)) {
            result = json_array_append_new(json, pyjansson_tuple2json(val));
        }
        else if (PyString_Check(val)) {
            result = json_array_append_new(json, json_string(PyString_AsString(val)));
        }
        else if (PyInt_Check(val)) {
            result = json_array_append_new(json, json_integer(PyInt_AsLong(val)));
        }
        else if (PyLong_Check(val)) {
            result = json_array_append_new(json, json_integer(PyLong_AsLong(val)));
        }
        else if (PyFloat_Check(val)) {
            result = json_array_append_new(json, json_real(PyFloat_AsDouble(val)));
        }
        else if (PyBool_Check(val)) {
            if (Py_False==val) {
                result = json_array_append_new(json, json_false());
            }
            else {
                result = json_array_append_new(json, json_true());
            }
        }
        else { // Py_None
            result = json_array_append_new(json, json_null());
        }
        if (-1 == result)
            goto failure;
    }
    return json;

failure:
    json_delete(json);
    return NULL;
}

/**
 * Convert PyTuple to JSONArray
 * @param PyTuple
 * @return JSONArray
 */
static json_t * pyjansson_tuple2json(PyObject *data) {
    PyObject *val;
    size_t size = PyTuple_Size(data);
    size_t i;
    
    json_t *json = json_array();
    int result;

    for (i=0;i<size;i++) {
        val = PyTuple_GetItem(data, i);

        if (PyDict_Check(val)) {
            result = json_array_append_new(json, pyjansson_dict2json(val));
        }
        else if (PyList_Check(val)) {
            result = json_array_append_new(json, pyjansson_list2json(val));
        }
        else if (PyTuple_Check(val)) {
            result = json_array_append_new(json, pyjansson_tuple2json(val));
        }
        else if (PyString_Check(val)) {
            result = json_array_append_new(json, json_string(PyString_AsString(val)));
        }
        else if (PyInt_Check(val)) {
            result = json_array_append_new(json, json_integer(PyInt_AsLong(val)));
        }
        else if (PyLong_Check(val)) {
            result = json_array_append_new(json, json_integer(PyLong_AsLong(val)));
        }
        else if (PyFloat_Check(val)) {
            result = json_array_append_new(json, json_real(PyFloat_AsDouble(val)));
        }
        else if (PyBool_Check(val)) {
            if (Py_False==val) {
                result = json_array_append_new(json, json_false());
            }
            else {
                result = json_array_append_new(json, json_true());
            }
        }
        else { // Py_None
            result = json_array_append_new(json, json_null());
        }
        if (-1 == result)
            goto failure;
    }
    return json;

failure:
    json_delete(json);
    return NULL;
}

/**
 * Convert dict or list or tuple to JSON string
 * @param dict | list | tuple
 * @return string
 */
static PyObject * pyjansson_dumps(PyObject *self, PyObject *args) {
    json_t *json;
    PyObject* data;
    int flags = JSON_ENCODE_ANY;
    if (!PyArg_ParseTuple(args, "O|i", &data, &flags)) {
        PyErr_SetString(PyExc_ValueError, "Invalid arguments");
        return NULL;
    }

    if (PyDict_Check(data)) {
        json = pyjansson_dict2json(data);
    }
    else if (PyList_Check(data)) {
        json = pyjansson_list2json(data);
    }
    else if (PyTuple_Check(data)) {
        json = pyjansson_tuple2json(data);
    }
    else if (flags & JSON_ENCODE_ANY) {
        if (PyString_Check(data)) {
            json = json_string(PyString_AsString(data));
        }
        else if (PyInt_Check(data)) {
            json = json_integer(PyInt_AsLong(data));
        }
        else if (PyLong_Check(data)) {
            json = json_integer(PyLong_AsLong(data));
        }
        else if (PyFloat_Check(data)) {
            json = json_real(PyFloat_AsDouble(data));
        }
        else if (PyBool_Check(data)) {
            if (Py_False==data) {
                json = json_false();
            }
            else {
                json = json_true();
            }
        }
        else { // Py_None
            json = json_null();
        }
    }
    else {
        PyErr_SetString(PyExc_ValueError, "Accept only dict, list or tuple.");
    }
    
    if (NULL!=json) {
        char *dump = json_dumps(json, flags);
        if (NULL!=dump) {
            json_delete(json);
            return PyString_FromString(dump);
        }
    }

    PyErr_SetString(JSON_EncodeError, "Error convert to JSON string");
    return NULL;
}

/**
 * Module methods
 */
static PyMethodDef pyjanssonMethods[] = {
    {"loads",  pyjansson_parse, METH_VARARGS, "Parse JSON string and return dict."},
    {"decode",  pyjansson_parse, METH_VARARGS, "Parse JSON string and return dict."},
    {"dumps",  pyjansson_dumps, METH_VARARGS, "Convert dict or list or tuple to JSON string."},
    {"encode",  pyjansson_dumps, METH_VARARGS, "Convert dict or list or tuple to JSON string."},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

/**
 * Init pyjansson module
 */
PyMODINIT_FUNC initpyjansson(void) {
    PyObject *m = Py_InitModule("pyjansson", pyjanssonMethods);
    if (NULL == m)
        return;

    JSON_Error = PyErr_NewException("pyjansson.Error", NULL, NULL);
    if (JSON_Error == NULL)
        return;
    Py_INCREF(JSON_Error);
    PyModule_AddObject(m, "Error", JSON_Error);

    JSON_EncodeError = PyErr_NewException("pyjansson.EncodeError", JSON_Error, NULL);
    if (JSON_EncodeError == NULL)
        return;
    Py_INCREF(JSON_EncodeError);
    PyModule_AddObject(m, "EncodeError", JSON_EncodeError);

    JSON_DecodeError = PyErr_NewException("pyjansson.DecodeError", JSON_Error, NULL);
    if (JSON_DecodeError == NULL)
        return;
    Py_INCREF(JSON_DecodeError);
    PyModule_AddObject(m, "DecodeError", JSON_DecodeError);
    
    // Set constants
    
    // decoding
    PyModule_AddIntConstant(m, "JSON_REJECT_DUPLICATES",JSON_REJECT_DUPLICATES);
    PyModule_AddIntConstant(m, "JSON_DISABLE_EOF_CHECK",JSON_DISABLE_EOF_CHECK);
    PyModule_AddIntConstant(m, "JSON_DECODE_ANY",       JSON_DECODE_ANY);
    
    // encoding
    PyModule_AddIntConstant(m, "JSON_COMPACT",          JSON_COMPACT);
    PyModule_AddIntConstant(m, "JSON_ENSURE_ASCII",     JSON_ENSURE_ASCII);
    PyModule_AddIntConstant(m, "JSON_PRESERVE_ORDER",   JSON_PRESERVE_ORDER);
    PyModule_AddIntConstant(m, "JSON_SORT_KEYS",        JSON_SORT_KEYS);
    PyModule_AddIntConstant(m, "JSON_ENCODE_ANY",       JSON_ENCODE_ANY);

    // Module version (the MODULE_VERSION macro is defined by setup.py)
    PyModule_AddStringConstant(m, "__version__", MODULE_VERSION);
}

#ifdef __cplusplus
}
#endif
