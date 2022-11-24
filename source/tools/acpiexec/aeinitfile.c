/******************************************************************************
 *
 * Module Name: aeinitfile - Support for optional initialization file
 *
 *****************************************************************************/

<<<<<<< HEAD   (d64c66 Import ACPICA 20200110 sources)
/*
 * Copyright (C) 2000 - 2020, Intel Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */

#include "aecommon.h"
#include "acdispat.h"

#define _COMPONENT          ACPI_TOOLS
        ACPI_MODULE_NAME    ("aeinitfile")


/* Local prototypes */

static void
AeEnterInitFileEntry (
    INIT_FILE_ENTRY         InitEntry,
    ACPI_WALK_STATE         *WalkState);


#define AE_FILE_BUFFER_SIZE     512

static char                 LineBuffer[AE_FILE_BUFFER_SIZE];
static char                 NameBuffer[AE_FILE_BUFFER_SIZE];
static char                 ValueBuffer[AE_FILE_BUFFER_SIZE];
static FILE                 *InitFile;


/******************************************************************************
 *
 * FUNCTION:    AeOpenInitializationFile
 *
 * PARAMETERS:  Filename            - Path to the init file
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Open the initialization file for the -fi option
 *
 *****************************************************************************/

int
AeOpenInitializationFile (
    char                    *Filename)
{

    InitFile = fopen (Filename, "r");
    if (!InitFile)
    {
        fprintf (stderr,
            "Could not open initialization file: %s\n", Filename);
        return (-1);
    }

    AcpiOsPrintf ("Opened initialization file [%s]\n", Filename);
    return (0);
}


/******************************************************************************
 *
 * FUNCTION:    AeProcessInitFile
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Read the initialization file and perform all namespace
 *              initializations. AcpiGbl_InitEntries will be used for region
 *              field initialization.
 *
 * NOTE:        The format of the file is multiple lines, each of format:
 *                  <ACPI-pathname> <Integer Value>
 *
 *****************************************************************************/

void
AeProcessInitFile(
    void)
{
    ACPI_WALK_STATE         *WalkState;
    UINT64                  idx;
    ACPI_STATUS             Status;
    char                    *Token;
    char                    *ObjectBuffer;
    char                    *TempNameBuffer;
    ACPI_OBJECT_TYPE        Type;
    ACPI_OBJECT             TempObject;


    if (!InitFile)
    {
        return;
    }

    /* Create needed objects to be reused for each init entry */

    WalkState = AcpiDsCreateWalkState (0, NULL, NULL, NULL);
    NameBuffer[0] = '\\';

    while (fgets (LineBuffer, AE_FILE_BUFFER_SIZE, InitFile) != NULL)
    {
        ++AcpiGbl_InitFileLineCount;
    }
    rewind (InitFile);

    AcpiGbl_InitEntries =
        AcpiOsAllocate (sizeof (INIT_FILE_ENTRY) * AcpiGbl_InitFileLineCount);
    for (idx = 0; fgets (LineBuffer, AE_FILE_BUFFER_SIZE, InitFile); ++idx)
    {

        TempNameBuffer = AcpiDbGetNextToken (LineBuffer, &Token, &Type);
        if (LineBuffer[0] == '\\')
        {
            strcpy (NameBuffer, TempNameBuffer);
        }
        else
        {
            /* Add a root prefix if not present in the string */

            strcpy (NameBuffer + 1, TempNameBuffer);
        }

        AcpiGbl_InitEntries[idx].Name =
            AcpiOsAllocateZeroed (strnlen (NameBuffer, AE_FILE_BUFFER_SIZE) + 1);

        strcpy (AcpiGbl_InitEntries[idx].Name, NameBuffer);

        ObjectBuffer = AcpiDbGetNextToken (Token, &Token, &Type);

        if (Type == ACPI_TYPE_FIELD_UNIT)
        {
            Status = AcpiDbConvertToObject (ACPI_TYPE_BUFFER, ObjectBuffer,
                &TempObject);
        }
        else
        {
            Status = AcpiDbConvertToObject (Type, ObjectBuffer, &TempObject);
        }

        Status = AcpiUtCopyEobjectToIobject (&TempObject,
            &AcpiGbl_InitEntries[idx].ObjDesc);

        if (Type == ACPI_TYPE_BUFFER || Type == ACPI_TYPE_FIELD_UNIT)
        {
            ACPI_FREE (TempObject.Buffer.Pointer);
        }

        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf ("%s %s\n", ValueBuffer,
                AcpiFormatException (Status));
            goto CleanupAndExit;
        }

        /*
         * Special case for field units. Field units are dependent on the
         * parent region. This parent region has yet to be created so defer the
         * initialization until the dispatcher. For all other types, initialize
         * the namespace node with the value found in the init file.
         */
        if (Type != ACPI_TYPE_FIELD_UNIT)
        {
            AeEnterInitFileEntry (AcpiGbl_InitEntries[idx], WalkState);
        }
    }

    /* Cleanup */

CleanupAndExit:
    fclose (InitFile);
    AcpiDsDeleteWalkState (WalkState);
}


/******************************************************************************
 *
 * FUNCTION:    AeInitFileEntry
 *
 * PARAMETERS:  InitEntry           - Entry of the init file
 *              WalkState           - Used for the Store operation
 *
 * RETURN:      None
 *
 * DESCRIPTION: Perform initialization of a single namespace object
 *
 *              Note: namespace of objects are limited to integers and region
 *              fields units of 8 bytes at this time.
 *
 *****************************************************************************/

static void
AeEnterInitFileEntry (
    INIT_FILE_ENTRY         InitEntry,
    ACPI_WALK_STATE         *WalkState)
{
    char                    *Pathname = InitEntry.Name;
    ACPI_OPERAND_OBJECT     *ObjDesc = InitEntry.ObjDesc;
    ACPI_NAMESPACE_NODE     *NewNode;
    ACPI_STATUS             Status;


    Status = AcpiNsLookup (NULL, Pathname, ObjDesc->Common.Type,
        ACPI_IMODE_LOAD_PASS2, ACPI_NS_ERROR_IF_FOUND | ACPI_NS_NO_UPSEARCH |
        ACPI_NS_EARLY_INIT, NULL, &NewNode);
    if (ACPI_FAILURE (Status))
    {
        ACPI_EXCEPTION ((AE_INFO, Status,
            "While creating name from namespace initialization file: %s",
            Pathname));
        return;
    }

    /* Store pointer to value descriptor in the Node */

    Status = AcpiNsAttachObject (NewNode, ObjDesc,
         ObjDesc->Common.Type);
    if (ACPI_FAILURE (Status))
    {
        ACPI_EXCEPTION ((AE_INFO, Status,
            "While attaching object to node from namespace initialization file: %s",
            Pathname));
        return;
    }

    /* Remove local reference to the object */

    AcpiUtRemoveReference (ObjDesc);
}


/******************************************************************************
 *
 * FUNCTION:    AeLookupInitFileEntry
 *
 * PARAMETERS:  Pathname            - AML namepath in external format
 *              ValueString         - value of the namepath if it exitst
 *
 * RETURN:      None
 *
 * DESCRIPTION: Search the init file for a particular name and its value.
 *
 *****************************************************************************/

ACPI_STATUS
AeLookupInitFileEntry (
    char                    *Pathname,
    ACPI_OPERAND_OBJECT     **ObjDesc)
{
    UINT32                  i;

    if (!AcpiGbl_InitEntries)
    {
        return AE_NOT_FOUND;
    }

    for (i = 0; i < AcpiGbl_InitFileLineCount; ++i)
    {
        if (!strcmp(AcpiGbl_InitEntries[i].Name, Pathname))
        {
            *ObjDesc = AcpiGbl_InitEntries[i].ObjDesc;
            return AE_OK;
        }
    }
    return AE_NOT_FOUND;
=======
/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2022, Intel Corp.
 * All rights reserved.
 *
*
 *****************************************************************************
 *
*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
*
 *****************************************************************************/

#include "aecommon.h"
#include "acdispat.h"
#include "acnamesp.h"


#define _COMPONENT          ACPI_TOOLS
        ACPI_MODULE_NAME    ("aeinitfile")


#define AE_FILE_BUFFER_SIZE     512

static char                 LineBuffer[AE_FILE_BUFFER_SIZE];
static char                 NameBuffer[AE_FILE_BUFFER_SIZE];
static FILE                 *InitFile;


/******************************************************************************
 *
 * FUNCTION:    AeOpenInitializationFile
 *
 * PARAMETERS:  Filename            - Path to the init file
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Open the initialization file for the -fi option
 *
 *****************************************************************************/

int
AeOpenInitializationFile (
    char                    *Filename)
{

    InitFile = fopen (Filename, "r");
    if (!InitFile)
    {
        fprintf (stderr,
            "Could not open initialization file: %s\n", Filename);
        return (-1);
    }

    AcpiOsPrintf ("Opened initialization file [%s]\n", Filename);
    return (0);
}


/******************************************************************************
 *
 * FUNCTION:    AeProcessInitFile
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Read the initialization file and perform all namespace
 *              initializations. AcpiGbl_InitEntries will be used for all
 *              object initialization.
 *
 * NOTE:        The format of the file is multiple lines, each of format:
 *                  <ACPI-pathname> <New Value>
 *
 *****************************************************************************/

ACPI_STATUS
AeProcessInitFile (
    void)
{
    ACPI_WALK_STATE         *WalkState;
    UINT64                  idx;
    ACPI_STATUS             Status = AE_OK;
    char                    *Token;
    char                    *ValueBuffer;
    char                    *TempNameBuffer;
    ACPI_OBJECT_TYPE        Type;
    ACPI_OBJECT             TempObject;


    if (!InitFile)
    {
        return (AE_OK);
    }

    /* Create needed objects to be reused for each init entry */

    WalkState = AcpiDsCreateWalkState (0, NULL, NULL, NULL);
    NameBuffer[0] = '\\';
    NameBuffer[1] = 0;

    while (fgets (LineBuffer, AE_FILE_BUFFER_SIZE, InitFile) != NULL)
    {
        ++AcpiGbl_InitFileLineCount;
    }
    rewind (InitFile);

    /*
     * Allocate and populate the Gbl_InitEntries array
     */
    AcpiGbl_InitEntries =
        AcpiOsAllocateZeroed (sizeof (INIT_FILE_ENTRY) * AcpiGbl_InitFileLineCount);
    for (idx = 0; fgets (LineBuffer, AE_FILE_BUFFER_SIZE, InitFile); ++idx)
    {
        TempNameBuffer = AcpiDbGetNextToken (LineBuffer, &Token, &Type);
        if (!TempNameBuffer)
        {
            AcpiGbl_InitEntries[idx].Name = NULL;
            continue;
        }

        if (LineBuffer[0] == '\\')
        {
            strcpy (NameBuffer, TempNameBuffer);
        }
        else
        {
            /* Add a root prefix if not present in the string */

            strcpy (NameBuffer + 1, TempNameBuffer);
        }

        AcpiNsNormalizePathname (NameBuffer);
        AcpiGbl_InitEntries[idx].Name =
            AcpiOsAllocateZeroed (strnlen (NameBuffer, AE_FILE_BUFFER_SIZE) + 1);
        strcpy (AcpiGbl_InitEntries[idx].Name, NameBuffer);

        ValueBuffer = AcpiDbGetNextToken (Token, &Token, &Type);
        if (!ValueBuffer)
        {
            AcpiGbl_InitEntries[idx].Value = NULL;
            continue;
        }

        AcpiGbl_InitEntries[idx].Value =
            AcpiOsAllocateZeroed (strnlen (ValueBuffer, AE_FILE_BUFFER_SIZE) + 1);
        strcpy (AcpiGbl_InitEntries[idx].Value, ValueBuffer);

        if (Type == ACPI_TYPE_FIELD_UNIT)
        {
            Status = AcpiDbConvertToObject (ACPI_TYPE_BUFFER, ValueBuffer,
                &TempObject);
        }
        else
        {
            Status = AcpiDbConvertToObject (Type, ValueBuffer, &TempObject);
        }

        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf ("%s[%s]: %s\n", NameBuffer, AcpiUtGetTypeName (Type),
                AcpiFormatException (Status));
            goto CleanupAndExit;
        }

        Status = AcpiUtCopyEobjectToIobject (&TempObject,
            &AcpiGbl_InitEntries[idx].ObjDesc);

        /* Cleanup the external object created by DbConvertToObject above */

        if (ACPI_SUCCESS (Status))
        {
            if (Type == ACPI_TYPE_BUFFER || Type == ACPI_TYPE_FIELD_UNIT)
            {
                ACPI_FREE (TempObject.Buffer.Pointer);
            }
            else if (Type == ACPI_TYPE_PACKAGE)
            {
                AcpiDbDeleteObjects (1, &TempObject);
            }
        }
        else
        {
            AcpiOsPrintf ("%s[%s]: %s\n", NameBuffer, AcpiUtGetTypeName (Type),
                AcpiFormatException (Status));
            goto CleanupAndExit;
        }

        /*
         * Initialize the namespace node with the value found in the init file.
         */
        AcpiOsPrintf ("Namespace object init from file: %16s, Value \"%s\", Type %s\n",
            AcpiGbl_InitEntries[idx].Name, AcpiGbl_InitEntries[idx].Value, AcpiUtGetTypeName (Type));
    }

    /* Cleanup */

CleanupAndExit:
    fclose (InitFile);
    AcpiDsDeleteWalkState (WalkState);
    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AeLookupInitFileEntry
 *
 * PARAMETERS:  Pathname            - AML namepath in external format
 *              ObjDesc             - Where the object is returned if it exists
 *
 * RETURN:      Status. AE_OK if a match was found
 *
 * DESCRIPTION: Search the init file for a particular name and its value.
 *
 *****************************************************************************/

ACPI_STATUS
AeLookupInitFileEntry (
    char                    *Pathname,
    ACPI_OPERAND_OBJECT     **ObjDesc)
{
    UINT32                  i;

    ACPI_FUNCTION_TRACE (AeLookupInitFileEntry);

    ACPI_DEBUG_PRINT ((ACPI_DB_NAMES, "Lookup: %s\n", Pathname));

    if (!AcpiGbl_InitEntries)
    {
        return (AE_NOT_FOUND);
    }

    AcpiNsNormalizePathname (Pathname);

    for (i = 0; i < AcpiGbl_InitFileLineCount; ++i)
    {
        if (AcpiGbl_InitEntries[i].Name &&
            !strcmp (AcpiGbl_InitEntries[i].Name, Pathname))
        {
            *ObjDesc = AcpiGbl_InitEntries[i].ObjDesc;
            AcpiGbl_InitEntries[i].IsUsed = TRUE;
            ACPI_DEBUG_PRINT ((ACPI_DB_NAMES, "Found match: %s, %p\n", Pathname, *ObjDesc));
            return_ACPI_STATUS (AE_OK);
        }
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_NAMES, "No match found: %s\n", Pathname));
    return_ACPI_STATUS (AE_NOT_FOUND);
}


/******************************************************************************
 *
 * FUNCTION:    AeDisplayUnusedInitFileItems
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display all init file items that have not been referenced
 *              (i.e., items that have not been found in the namespace).
 *
 *****************************************************************************/

void
AeDisplayUnusedInitFileItems (
    void)
{
    UINT32                  i;


    if (!AcpiGbl_InitEntries)
    {
        return;
    }

    for (i = 0; i < AcpiGbl_InitFileLineCount; ++i)
    {
        if (AcpiGbl_InitEntries[i].Name &&
            !AcpiGbl_InitEntries[i].IsUsed)
        {
            AcpiOsPrintf ("Init file entry not found in namespace "
                "(or is a non-data type): %s\n",
                AcpiGbl_InitEntries[i].Name);
        }
    }
}


/******************************************************************************
 *
 * FUNCTION:    AeDeleteInitFileList
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Delete the global namespace initialization file data
 *
 *****************************************************************************/

void
AeDeleteInitFileList (
    void)
{
    UINT32                  i;


    if (!AcpiGbl_InitEntries)
    {
        return;
    }

    for (i = 0; i < AcpiGbl_InitFileLineCount; ++i)
    {

        if ((AcpiGbl_InitEntries[i].ObjDesc) && (AcpiGbl_InitEntries[i].Value))
        {
            /* Remove one reference on the object (and all subobjects) */

            AcpiUtRemoveReference (AcpiGbl_InitEntries[i].ObjDesc);
        }
    }

    AcpiOsFree (AcpiGbl_InitEntries);
>>>>>>> BRANCH (a8f750 Project import generated by Copybara.)
}
