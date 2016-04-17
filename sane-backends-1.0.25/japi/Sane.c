/* sane - Scanner Access Now Easy.
   Copyright (C) 1997 Jeffrey S. Freedman
   This file is part of the SANE package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.

   As a special exception, the authors of SANE give permission for
   additional uses of the libraries contained in this release of SANE.

   The exception is that, if you link a SANE library with other files
   to produce an executable, this does not by itself cause the
   resulting executable to be covered by the GNU General Public
   License.  Your use of that executable is in no way restricted on
   account of linking the SANE library code into it.

   This exception does not, however, invalidate any other reasons why
   the executable file might be covered by the GNU General Public
   License.

   If you submit changes to SANE to the maintainers to be included in
   a subsequent release, you agree by submitting the changes that
   those changes may be distributed with this exception intact.

   If you write modifications of your own for SANE, it is your choice
   whether to permit this exception to apply to your modifications.
   If you do not wish that, delete this exception notice.  */

/**
 **	Sane.c - Native methods for the SANE Java API.
 **
 **	Written: 10/9/97 - JSF
 **/

#include "Sane.h"
#include <sane/sane.h>
#include <string.h>

#include <stdio.h>	/* Debugging */

#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     Sane
 * Method:    init
 * Signature: ([I)I
 */
JNIEXPORT jint JNICALL Java_Sane_init
  (JNIEnv *env, jobject jobj, jintArray versionCode)
	{
	jsize len;			/* Gets array length. */
	jint *versionCodeBody;		/* Gets ->array. */
	SANE_Int version;		/* Gets version. */
	SANE_Status status;		/* Get SANE return. */

	status = sane_init(&version, 0);
	len = (*env)->GetArrayLength(env, versionCode);
	versionCodeBody = (*env)->GetIntArrayElements(env, versionCode, 0);
	if (len > 0)			/* Return version. */
		versionCodeBody[0] = version;
	(*env)->ReleaseIntArrayElements(env, versionCode, versionCodeBody, 0);
	return (status);
	}
/*
 * Class:     Sane
 * Method:    exit
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_Sane_exit
  (JNIEnv *env, jobject jobj)
	{
	sane_exit();
	}


/*
 * Class:     Sane
 * Method:    getDevices
 * Signature: ([LSaneDevice;Z)I
 */
JNIEXPORT jint JNICALL Java_Sane_getDevicesNative
  (JNIEnv *env, jobject jobj, jobjectArray devList, jboolean localOnly)
	{
					/* Gets device list. */
	const SANE_Device **device_list;
	SANE_Status status;		/* Gets status. */
	int devListLen;			/* Gets length of devList. */
	jobject devObj;			/* Gets each SaneDevice object. */
	jclass devClass;		/* Gets SaneDevice class. */
	jfieldID fid;			/* Gets each field ID. */
	int i;

					/* Get the list. */
	status = sane_get_devices(&device_list, localOnly);	
	if (status != SANE_STATUS_GOOD)
		return (status);
					/* Get length of Java array. */
	devListLen = (*env)->GetArrayLength(env, devList);
					/* Return devices to user. */
	for (i = 0; i < devListLen - 1 && device_list[i]; i++)
		{
					/* Get Java object, class. */
		devObj = (*env)->GetObjectArrayElement(env, devList, i);
		devClass = (*env)->GetObjectClass(env, devObj);
					/* Fill in each member. */
		fid = (*env)->GetFieldID(env, devClass, "name",
							"Ljava/lang/String;");
		(*env)->SetObjectField(env, devObj, fid,
			(*env)->NewStringUTF(env, device_list[i]->name));
		fid = (*env)->GetFieldID(env, devClass, "vendor",
							"Ljava/lang/String;");
		(*env)->SetObjectField(env, devObj, fid,
			(*env)->NewStringUTF(env, device_list[i]->vendor));
		fid = (*env)->GetFieldID(env, devClass, "model",
							"Ljava/lang/String;");
		(*env)->SetObjectField(env, devObj, fid,
			(*env)->NewStringUTF(env, device_list[i]->model));
		fid = (*env)->GetFieldID(env, devClass, "type",
							"Ljava/lang/String;");
		(*env)->SetObjectField(env, devObj, fid,
			(*env)->NewStringUTF(env, device_list[i]->type));
		}
					/* End list with a null. */
	(*env)->SetObjectArrayElement(env, devList, i, 0);
	return (status);
	}

/*
 * Class:     Sane
 * Method:    open
 * Signature: (Ljava/lang/String;[J)I
 */
JNIEXPORT jint JNICALL Java_Sane_open
  (JNIEnv *env, jobject jobj, jstring deviceName, jintArray handle)
	{
	SANE_Handle sane_handle;	/* Gets handle. */
	jint s_handle;
	const char *device_name;	/* Gets dev. name. */
	int status;			/* Gets return code. */

	device_name = (*env)->GetStringUTFChars(env, deviceName, 0);
					/* Open it. */
	status = sane_open(device_name, &sane_handle);
	(*env)->ReleaseStringUTFChars(env, deviceName, device_name);
					/* Return handle. */
	s_handle = (jint) sane_handle;
	(*env)->SetIntArrayRegion(env, handle, 0, 1, &s_handle);
	return (status);
	}

/*
 * Class:     Sane
 * Method:    close
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_Sane_close
  (JNIEnv *env, jobject jobj, jint handle)
	{
	sane_close((SANE_Handle) handle);
	}

/*
 * Class:     Sane
 * Method:    getOptionNative
 * Signature: (IILSaneOption;)V
 */
JNIEXPORT void JNICALL Java_Sane_getOptionNative
  (JNIEnv *env, jobject jobj, jint handle, jint option, jobject optObj)
	{
	jclass optClass;		/* Gets its class. */
	jfieldID fid;			/* Gets each field ID. */
	jstring str;			/* Gets strings. */

					/* Get info from sane. */
	const SANE_Option_Descriptor *sopt = sane_get_option_descriptor(
			(SANE_Handle) handle, option);
					/* Get class info. */
	optClass = (*env)->GetObjectClass(env, optObj);
					/* Fill in each member. */
	fid = (*env)->GetFieldID(env, optClass, "name", "Ljava/lang/String;");
	if (!sopt)			/* Failed. */
		{			/* Set name to null. */
		(*env)->SetObjectField(env, optObj, fid, 0);
		return;
		}
					/* Return name. */
	(*env)->SetObjectField(env, optObj, fid,
				(*env)->NewStringUTF(env, sopt->name));
					/* Return title. */
	fid = (*env)->GetFieldID(env, optClass, "title", "Ljava/lang/String;");
	str = sopt->title ? (*env)->NewStringUTF(env, sopt->title) : 0;
	(*env)->SetObjectField(env, optObj, fid, str);
					/* Return descr. */
	fid = (*env)->GetFieldID(env, optClass, "desc", "Ljava/lang/String;");
	(*env)->SetObjectField(env, optObj, fid,
				(*env)->NewStringUTF(env, sopt->desc));
					/* Return type. */
	fid = (*env)->GetFieldID(env, optClass, "type", "I");
	(*env)->SetIntField(env, optObj, fid, sopt->type);
					/* Return unit. */
	fid = (*env)->GetFieldID(env, optClass, "unit", "I");
	(*env)->SetIntField(env, optObj, fid, sopt->unit);
					/* Return size. */
	fid = (*env)->GetFieldID(env, optClass, "size", "I");
	(*env)->SetIntField(env, optObj, fid, sopt->size);
					/* Return cap. */
	fid = (*env)->GetFieldID(env, optClass, "cap", "I");
	(*env)->SetIntField(env, optObj, fid, sopt->cap);
					/* Return constraint_type. */
	fid = (*env)->GetFieldID(env, optClass, "constraintType", "I");
	(*env)->SetIntField(env, optObj, fid, sopt->constraint_type);
	/*
	 *	Now for the constraint itself.
	 */
	if (sopt->constraint_type == SANE_CONSTRAINT_RANGE)
		{
					/* Create range object. */
		jclass rangeClass = (*env)->FindClass(env, "SaneRange");
		jobject range = (*env)->AllocObject(env, rangeClass);
					/* Fill in fields. */
		fid = (*env)->GetFieldID(env, rangeClass, "min", "I");
		(*env)->SetIntField(env, range, fid, 
						sopt->constraint.range->min);
		fid = (*env)->GetFieldID(env, rangeClass, "max", "I");
		(*env)->SetIntField(env, range, fid, 
						sopt->constraint.range->max);
		fid = (*env)->GetFieldID(env, rangeClass, "quant", "I");
		(*env)->SetIntField(env, range, fid, 
						sopt->constraint.range->quant);
		fid = (*env)->GetFieldID(env, optClass, "rangeConstraint",
						"LSaneRange;");
					/* Store range. */
		(*env)->SetObjectField(env, optObj, fid, range);
		}
	else if (sopt->constraint_type == SANE_CONSTRAINT_WORD_LIST)
		{			/* Get array of integers. */
		jintArray wordList;
		jint *elements;
		int i;
					/* First word. is the length.	*/
		wordList = (*env)->NewIntArray(env, 
				sopt->constraint.word_list[0]);
					/* Copy in the integers.	*/
		elements = (*env)->GetIntArrayElements(env, wordList, 0);
		for (i = 0; i < sopt->constraint.word_list[0]; i++)
			elements[i] = sopt->constraint.word_list[i];
		(*env)->ReleaseIntArrayElements(env, wordList, elements, 0);
					/* Set the field. */
		fid = (*env)->GetFieldID(env, optClass, "wordListConstraint",
									"[I");
		(*env)->SetObjectField(env, optObj, fid, wordList);
		}
	else if (sopt->constraint_type == SANE_CONSTRAINT_STRING_LIST)
		{
		jclass stringClass = (*env)->FindClass(env, "java/lang/String");
		jobjectArray stringList;
		int len;		/* Gets # elements */
		int i;

		for (len = 0; sopt->constraint.string_list[len]; len++)
			;
		stringList = (*env)->NewObjectArray(env, len + 1, 
							stringClass, 0);
					/* Add each string. */
		for (i = 0; i < len; i++)
			{
			(*env)->SetObjectArrayElement(env, stringList, i,
				(*env)->NewStringUTF(env,
					sopt->constraint.string_list[i]));
			}
					/* 0 at end. */
		(*env)->SetObjectArrayElement(env, stringList, len, 0);
					/* Set the field. */
		fid = (*env)->GetFieldID(env, optClass, 
			"stringListConstraint", "[Ljava/lang/String;");
		(*env)->SetObjectField(env, optObj, fid, stringList);
		}
	}

/*
 * Class:     Sane
 * Method:    getControlOption
 * Signature: (II[I[I)I
 */
JNIEXPORT jint JNICALL Java_Sane_getControlOption__II_3I_3I
  (JNIEnv *env, jobject jobj, jint handle, jint option, jintArray value, 
						jintArray info)
	{
	SANE_Status status;		/* Gets status. */
	SANE_Int i;			/* Gets info. passed back. */
	int v;

	status = sane_control_option((SANE_Handle) handle, option,
			SANE_ACTION_GET_VALUE, &v, &i);
	if (value)
		(*env)->SetIntArrayRegion(env, value, 0, 1, &v);
	if (info)
		(*env)->SetIntArrayRegion(env, info, 0, 1, &i);
	return (status);
	}

/*
 * Class:     Sane
 * Method:    getControlOption
 * Signature: (II[B[I)I
 */
JNIEXPORT jint JNICALL Java_Sane_getControlOption__II_3B_3I
  (JNIEnv *env, jobject jobj, jint handle, jint option, jbyteArray value, 
						jintArray info)
	{
	SANE_Status status;		/* Gets status. */
	SANE_Int i;			/* Gets info. passed back. */
	char *str;

	str = (*env)->GetByteArrayElements(env, value, 0);
	status = sane_control_option((SANE_Handle) handle, option,
			SANE_ACTION_GET_VALUE, str, &i);
	(*env)->ReleaseByteArrayElements(env, value, str, 0);
	if (info)
		(*env)->SetIntArrayRegion(env, info, 0, 1, &i);
	return (status);
	}

/*
 * Class:     Sane
 * Method:    setControlOption
 * Signature: (IIII[I)I
 */
JNIEXPORT jint JNICALL Java_Sane_setControlOption__IIII_3I
  (JNIEnv *env, jobject jobj, jint handle, jint option, jint action, 
				jint value, jintArray info)
	{
	SANE_Status status;		/* Gets status. */
	SANE_Int i;			/* Gets info. passed back. */
	status = sane_control_option((SANE_Handle) handle, option, action,
					&value, &i);
	if (info)
		(*env)->SetIntArrayRegion(env, info, 0, 1, &i);
	return (status);
	}

/*
 *	Get string length.  This exists because sometimes strings seem to be
 *	padded with negatives.
 */

static int String_length
	(
	const char *str
	)
	{
	const char *ptr;
	for (ptr = str; *ptr > 0; ptr++)
		;
	return ((int) (ptr - str));
	}

/*
 * Class:     Sane
 * Method:    setControlOption
 * Signature: (IIILjava/lang/String;[I)I
 */
JNIEXPORT jint JNICALL Java_Sane_setControlOption__IIILjava_lang_String_2_3I
  (JNIEnv *env, jobject jobj, jint handle, jint option, jint action,
				jstring value, jintArray info)
	{
	SANE_Status status;		/* Gets status. */
	SANE_Int i;			/* Gets info. passed back. */
	const char *valuep;
	char buf[512];			/* Hope this is big enough. */
	int len;			/* Gets string length. */

	valuep = (*env)->GetStringUTFChars(env, value, 0);
	len = String_length(valuep);
	if (len >= sizeof(buf))
		len = sizeof(buf) - 1;
	strncpy(buf, valuep, len);
	buf[len] = 0;			/* Insure it's 0-delimited. */
	status = sane_control_option((SANE_Handle) handle, option, action,
					(void *) &buf[0], &i);
					/* +++++++Want to return new val? */
	(*env)->ReleaseStringUTFChars(env, value, valuep);
	if (info)
		(*env)->SetIntArrayRegion(env, info, 0, 1, &i);
	return (status);
	}

/*
 * Class:     Sane
 * Method:    getParameters
 * Signature: (ILSaneParameters;)I
 */
JNIEXPORT jint JNICALL Java_Sane_getParameters
  (JNIEnv *env, jobject jobj, jint handle, jobject paramsObj)
	{
	SANE_Status status;		/* Gets status.	*/
	SANE_Parameters params;		/* Gets params. */
	jclass paramsClass;		/* Gets its class. */
	jfieldID fid;			/* Gets each field ID. */

	status = sane_get_parameters((SANE_Handle) handle, &params);
					/* Get class info. */
	paramsClass = (*env)->GetObjectClass(env, paramsObj);
					/* Fill in each member. */
	fid = (*env)->GetFieldID(env, paramsClass, "format", "I");
	(*env)->SetIntField(env, paramsObj, fid, params.format);
	fid = (*env)->GetFieldID(env, paramsClass, "lastFrame", "Z");
	(*env)->SetBooleanField(env, paramsObj, fid, params.last_frame);
	fid = (*env)->GetFieldID(env, paramsClass, "bytesPerLine", "I");
	(*env)->SetIntField(env, paramsObj, fid, params.bytes_per_line);
	fid = (*env)->GetFieldID(env, paramsClass, "pixelsPerLine", "I");
	(*env)->SetIntField(env, paramsObj, fid, params.pixels_per_line);
	fid = (*env)->GetFieldID(env, paramsClass, "lines", "I");
	(*env)->SetIntField(env, paramsObj, fid, params.lines);
	fid = (*env)->GetFieldID(env, paramsClass, "depth", "I");
	(*env)->SetIntField(env, paramsObj, fid, params.depth);
	return (status);
	}

/*
 * Class:     Sane
 * Method:    start
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL Java_Sane_start
  (JNIEnv *env, jobject jobj, jint handle)
	{
	return (sane_start((SANE_Handle) handle));
	}

/*
 * Class:     Sane
 * Method:    read
 * Signature: (I[BI[I)I
 */
JNIEXPORT jint JNICALL Java_Sane_read
  (JNIEnv *env, jobject jobj, jint handle, jbyteArray data, jint maxLength,
					jintArray length)
	{
	int status;
	jbyte *dataElements;
	int read_len;			/* # bytes read. */

					/* Get actual data ptr. */
	dataElements = (*env)->GetByteArrayElements(env, data, 0);
					/* Do the read. */
	status = sane_read((SANE_Handle) handle, dataElements, 
						maxLength, &read_len);
	(*env)->ReleaseByteArrayElements(env, data, dataElements, 0);
					/* Return # bytes read. */
	(*env)->SetIntArrayRegion(env, length, 0, 1, &read_len);
	return (status);
	}

/*
 * Class:     Sane
 * Method:    cancel
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_Sane_cancel
  (JNIEnv *env, jobject jobj, jint handle)
	{
	sane_cancel((SANE_Handle) handle);
	}

/*
 * Class:     Sane
 * Method:    strstatus
 * Signature: (I)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_Sane_strstatus
  (JNIEnv *env, jobject jobj, jint status)
	{
	const char *str = sane_strstatus(status);
	return ((*env)->NewStringUTF(env, str));
	}

#ifdef __cplusplus
}
#endif
