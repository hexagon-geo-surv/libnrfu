// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (C) 2022 Leica Geosystems AG
 */

#include <Python.h>
#include <nrfu.h>

enum {
	nrfu_LOG_LEVEL_SILENT,
	nrfu_LOG_LEVEL_ERROR,
	nrfu_LOG_LEVEL_INFO,
	nrfu_LOG_LEVEL_DEBUG,
};

PyDoc_STRVAR(update_doc,
"update() -> None\n"
"\n"
"Update a NRF5 device connected to given console.\n");

static PyObject *nrfu_Update(PyObject *self, PyObject *args, PyObject *kwds)
{
	static char *kwlist[] = { "device",
				  "init_packet",
				  "firmware",
				  "log_level",
				  NULL };

	const char *device, *init_packet, *firmware;
	int ret, log_level = nrfu_LOG_LEVEL_ERROR;
	enum nrfu_log_level lib_log_level;

	ret = PyArg_ParseTupleAndKeywords(args, kwds, "sss|i", kwlist,
					  &device, &init_packet, &firmware, &log_level);
	if (!ret)
		return NULL;

	switch (log_level) {
	case nrfu_LOG_LEVEL_SILENT:
		lib_log_level = NRFU_LOG_LEVEL_SILENT;
		break;
	case nrfu_LOG_LEVEL_ERROR:
		lib_log_level = NRFU_LOG_LEVEL_ERROR;
		break;
	case nrfu_LOG_LEVEL_INFO:
		lib_log_level = NRFU_LOG_LEVEL_INFO;
		break;
	case nrfu_LOG_LEVEL_DEBUG:
		lib_log_level = NRFU_LOG_LEVEL_DEBUG;
		break;
	}

	if (nrfu_update(device, init_packet, firmware, lib_log_level) < 0) {
		PyErr_Format(PyExc_ValueError, "Update failed!");
		return NULL;
	}

	Py_RETURN_NONE;
}

static PyMethodDef nrfu_module_methods[] = {
	{
		.ml_name = "update",
		.ml_meth = (PyCFunction)(void (*)(void))nrfu_Update,
		.ml_flags = METH_VARARGS | METH_KEYWORDS,
		.ml_doc = update_doc,
	},
	{ }
};

PyDoc_STRVAR(nrfu_Module_doc,
"Python bindings for libnrfu.\n\
\n\
This module wraps the native C libnrfu into a python class.");

static struct PyModuleDef nrfu_Module = {
	PyModuleDef_HEAD_INIT,
	.m_name = "nrfu",
	.m_doc = nrfu_Module_doc,
	.m_size = -1,
	.m_methods = nrfu_module_methods,
};

struct nrfu_ModuleConst {
	const char *name;
	int value;
};

static struct nrfu_ModuleConst nrfu_ModuleConsts[] = {
	{
		.name = "LOG_LEVEL_SILENT",
		.value = nrfu_LOG_LEVEL_SILENT,
	},
	{
		.name = "LOG_LEVEL_ERROR",
		.value = nrfu_LOG_LEVEL_ERROR,
	},
	{
		.name = "LOG_LEVEL_INFO",
		.value = nrfu_LOG_LEVEL_INFO,
	},
	{
		.name = "LOG_LEVEL_DEBUG",
		.value = nrfu_LOG_LEVEL_DEBUG,
	},
	{ }
};

PyMODINIT_FUNC PyInit_nrfu(void)
{
	PyObject *module;
	struct nrfu_ModuleConst *mod_const;
	int ret, i;

	module = PyModule_Create(&nrfu_Module);
	if (!module)
		return NULL;

	for (i = 0; nrfu_ModuleConsts[i].name; i++) {
		mod_const = &nrfu_ModuleConsts[i];

		ret = PyModule_AddIntConstant(module,
					     mod_const->name, mod_const->value);
		if (ret < 0)
			break;
	}

	if (ret < 0)
		return NULL;

	return module;
}
