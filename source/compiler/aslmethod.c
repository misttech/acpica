/******************************************************************************
 *
 * Module Name: aslmethod.c - Control method analysis walk
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

#include "aslcompiler.h"
#include "aslcompiler.y.h"
#include "acparser.h"
#include "amlcode.h"


#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslmethod")


/* Local prototypes */

static void
MtCheckNamedObjectInMethod (
    ACPI_PARSE_OBJECT       *Op,
    ASL_METHOD_INFO         *MethodInfo);


/*******************************************************************************
 *
 * FUNCTION:    MtMethodAnalysisWalkBegin
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Descending callback for the analysis walk. Check methods for:
 *              1) Initialized local variables
 *              2) Valid arguments
 *              3) Return types
 *
 ******************************************************************************/

ACPI_STATUS
MtMethodAnalysisWalkBegin (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ASL_ANALYSIS_WALK_INFO  *WalkInfo = (ASL_ANALYSIS_WALK_INFO *) Context;
    ASL_METHOD_INFO         *MethodInfo = WalkInfo->MethodStack;
    ACPI_PARSE_OBJECT       *Next;
    UINT32                  RegisterNumber;
    UINT32                  i;
    char                    LocalName[] = "Local0";
    char                    ArgName[] = "Arg0";
    ACPI_PARSE_OBJECT       *ArgNode;
    ACPI_PARSE_OBJECT       *NextType;
    ACPI_PARSE_OBJECT       *NextParamType;
    UINT8                   ActualArgs = 0;
    BOOLEAN                 HidExists;
    BOOLEAN                 AdrExists;


    /* Build cross-reference output file if requested */

    if (AslGbl_CrossReferenceOutput)
    {
        OtXrefWalkPart1 (Op, Level, MethodInfo);
    }

    switch (Op->Asl.ParseOpcode)
    {
    case PARSEOP_METHOD:

        AslGbl_TotalMethods++;

        /* Create and init method info */

        MethodInfo = UtLocalCalloc (sizeof (ASL_METHOD_INFO));
        MethodInfo->Next = WalkInfo->MethodStack;
        MethodInfo->Op = Op;

        WalkInfo->MethodStack = MethodInfo;

        /*
         * Special handling for _PSx methods. Dependency rules (same scope):
         *
         * 1) _PS0 - One of these must exist: _PS1, _PS2, _PS3
         * 2) _PS1/_PS2/_PS3: A _PS0 must exist
         */
        if (ACPI_COMPARE_NAMESEG (METHOD_NAME__PS0, Op->Asl.NameSeg))
        {
            /* For _PS0, one of _PS1/_PS2/_PS3 must exist */

            if ((!ApFindNameInScope (METHOD_NAME__PS1, Op)) &&
                (!ApFindNameInScope (METHOD_NAME__PS2, Op)) &&
                (!ApFindNameInScope (METHOD_NAME__PS3, Op)))
            {
                AslError (ASL_WARNING, ASL_MSG_MISSING_DEPENDENCY, Op,
                    "_PS0 requires one of _PS1/_PS2/_PS3 in same scope");
            }
        }
        else if (
            ACPI_COMPARE_NAMESEG (METHOD_NAME__PS1, Op->Asl.NameSeg) ||
            ACPI_COMPARE_NAMESEG (METHOD_NAME__PS2, Op->Asl.NameSeg) ||
            ACPI_COMPARE_NAMESEG (METHOD_NAME__PS3, Op->Asl.NameSeg))
        {
            /* For _PS1/_PS2/_PS3, a _PS0 must exist */

            if (!ApFindNameInScope (METHOD_NAME__PS0, Op))
            {
                sprintf (AslGbl_MsgBuffer,
                    "%4.4s requires _PS0 in same scope", Op->Asl.NameSeg);

                AslError (ASL_WARNING, ASL_MSG_MISSING_DEPENDENCY, Op,
                    AslGbl_MsgBuffer);
            }
        }

        /* Get the name node */

        Next = Op->Asl.Child;

        /* Get the NumArguments node */

        Next = Next->Asl.Next;
        MethodInfo->NumArguments = (UINT8)
            (((UINT8) Next->Asl.Value.Integer) & 0x07);

        /* Get the SerializeRule and SyncLevel nodes, ignored here */

        Next = Next->Asl.Next;
        MethodInfo->ShouldBeSerialized = (UINT8) Next->Asl.Value.Integer;

        Next = Next->Asl.Next;
        ArgNode = Next;

        /* Get the ReturnType node */

        Next = Next->Asl.Next;

        NextType = Next->Asl.Child;
        while (NextType)
        {
            /* Get and map each of the ReturnTypes */

            MethodInfo->ValidReturnTypes |= AnMapObjTypeToBtype (NextType);
            NextType->Asl.ParseOpcode = PARSEOP_DEFAULT_ARG;
            NextType = NextType->Asl.Next;
        }

        /* Get the ParameterType node */

        Next = Next->Asl.Next;

        NextType = Next->Asl.Child;
        while (NextType)
        {
            if (NextType->Asl.ParseOpcode == PARSEOP_DEFAULT_ARG)
            {
                NextParamType = NextType->Asl.Child;
                while (NextParamType)
                {
                    MethodInfo->ValidArgTypes[ActualArgs] |=
                        AnMapObjTypeToBtype (NextParamType);

                    NextParamType->Asl.ParseOpcode = PARSEOP_DEFAULT_ARG;
                    NextParamType = NextParamType->Asl.Next;
                }
            }
            else
            {
                MethodInfo->ValidArgTypes[ActualArgs] =
                    AnMapObjTypeToBtype (NextType);

                NextType->Asl.ParseOpcode = PARSEOP_DEFAULT_ARG;
                ActualArgs++;
            }

            NextType = NextType->Asl.Next;
        }

        if ((MethodInfo->NumArguments) &&
            (MethodInfo->NumArguments != ActualArgs))
        {
            /* error: Param list did not match number of args */
        }

        /* Allow numarguments == 0 for Function() */

        if ((!MethodInfo->NumArguments) && (ActualArgs))
        {
            MethodInfo->NumArguments = ActualArgs;
            ArgNode->Asl.Value.Integer |= ActualArgs;
        }

        /*
         * Actual arguments are initialized at method entry.
         * All other ArgX "registers" can be used as locals, so we
         * track their initialization.
         */
        for (i = 0; i < MethodInfo->NumArguments; i++)
        {
            MethodInfo->ArgInitialized[i] = TRUE;
        }
        break;

    case PARSEOP_METHODCALL:

        /* Check for a recursive method call */

        if (MethodInfo &&
           (Op->Asl.Node == MethodInfo->Op->Asl.Node))
        {
            if (MethodInfo->CreatesNamedObjects)
            {
                /*
                 * This is an error, as it will fail at runtime on all ACPI
                 * implementations. Any named object declarations will be
                 * executed twice, causing failure the second time. Note,
                 * this is independent of whether the method is declared
                 * Serialized, because the same thread is attempting to
                 * reenter the method, and this will always succeed.
                 */
                AslDualParseOpError (ASL_ERROR, ASL_MSG_ILLEGAL_RECURSION, Op,
                    Op->Asl.Value.String, ASL_MSG_FOUND_HERE, MethodInfo->Op,
                    MethodInfo->Op->Asl.ExternalName);
            }
            else
            {
                /* Method does not create objects, issue a remark */

                AslError (ASL_REMARK, ASL_MSG_RECURSION, Op, Op->Asl.ExternalName);
            }
        }
        break;

    case PARSEOP_LOCAL0:
    case PARSEOP_LOCAL1:
    case PARSEOP_LOCAL2:
    case PARSEOP_LOCAL3:
    case PARSEOP_LOCAL4:
    case PARSEOP_LOCAL5:
    case PARSEOP_LOCAL6:
    case PARSEOP_LOCAL7:

        if (!MethodInfo)
        {
            /*
             * Local was used outside a control method, or there was an error
             * in the method declaration.
             */
            AslError (ASL_REMARK, ASL_MSG_LOCAL_OUTSIDE_METHOD,
                Op, Op->Asl.ExternalName);
            return (AE_ERROR);
        }

        RegisterNumber = (Op->Asl.AmlOpcode & 0x0007);

        /*
         * If the local is being used as a target, mark the local
         * initialized
         */
        if (Op->Asl.CompileFlags & OP_IS_TARGET)
        {
            MethodInfo->LocalInitialized[RegisterNumber] = TRUE;
        }

        /*
         * Otherwise, this is a reference, check if the local
         * has been previously initialized.
         *
         * The only operator that accepts an uninitialized value is ObjectType()
         */
        else if ((!MethodInfo->LocalInitialized[RegisterNumber]) &&
                 (Op->Asl.Parent->Asl.ParseOpcode != PARSEOP_OBJECTTYPE))
        {
            LocalName[strlen (LocalName) -1] = (char) (RegisterNumber + 0x30);
            AslError (ASL_ERROR, ASL_MSG_LOCAL_INIT, Op, LocalName);
        }
        break;

    case PARSEOP_ARG0:
    case PARSEOP_ARG1:
    case PARSEOP_ARG2:
    case PARSEOP_ARG3:
    case PARSEOP_ARG4:
    case PARSEOP_ARG5:
    case PARSEOP_ARG6:

        if (!MethodInfo)
        {
            /*
             * Arg was used outside a control method, or there was an error
             * in the method declaration.
             */
            AslError (ASL_REMARK, ASL_MSG_LOCAL_OUTSIDE_METHOD,
                Op, Op->Asl.ExternalName);
            return (AE_ERROR);
        }

        RegisterNumber = (Op->Asl.AmlOpcode & 0x000F) - 8;
        ArgName[strlen (ArgName) -1] = (char) (RegisterNumber + 0x30);

        /*
         * If the Arg is being used as a target, mark the local
         * initialized
         */
        if (Op->Asl.CompileFlags & OP_IS_TARGET)
        {
            MethodInfo->ArgInitialized[RegisterNumber] = TRUE;
        }

        /*
         * Otherwise, this is a reference, check if the Arg
         * has been previously initialized.
         *
         * The only operator that accepts an uninitialized value is ObjectType()
         */
        else if ((!MethodInfo->ArgInitialized[RegisterNumber]) &&
            (Op->Asl.Parent->Asl.ParseOpcode != PARSEOP_OBJECTTYPE))
        {
            AslError (ASL_ERROR, ASL_MSG_ARG_INIT, Op, ArgName);
        }

        /* Flag this arg if it is not a "real" argument to the method */

        if (RegisterNumber >= MethodInfo->NumArguments)
        {
            AslError (ASL_REMARK, ASL_MSG_NOT_PARAMETER, Op, ArgName);
        }
        break;

    case PARSEOP_RETURN:

        if (!MethodInfo)
        {
            /*
             * Probably was an error in the method declaration,
             * no additional error here
             */
            ACPI_WARNING ((AE_INFO, "%p, No parent method", Op));
            return (AE_ERROR);
        }

        /*
         * A child indicates a possible return value. A simple Return or
         * Return() is marked with OP_IS_NULL_RETURN by the parser so
         * that it is not counted as a "real" return-with-value, although
         * the AML code that is actually emitted is Return(0). The AML
         * definition of Return has a required parameter, so we are
         * forced to convert a null return to Return(0).
         */
        if ((Op->Asl.Child) &&
            (Op->Asl.Child->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG) &&
            (!(Op->Asl.Child->Asl.CompileFlags & OP_IS_NULL_RETURN)))
        {
            MethodInfo->NumReturnWithValue++;
        }
        else
        {
            MethodInfo->NumReturnNoValue++;
        }
        break;

    case PARSEOP_BREAK:
    case PARSEOP_CONTINUE:

        Next = Op->Asl.Parent;
        while (Next)
        {
            if (Next->Asl.ParseOpcode == PARSEOP_WHILE)
            {
                break;
            }
            Next = Next->Asl.Parent;
        }

        if (!Next)
        {
            AslError (ASL_ERROR, ASL_MSG_NO_WHILE, Op, NULL);
        }
        break;

    case PARSEOP_STALL:

        /* We can range check if the argument is an integer */

        if ((Op->Asl.Child->Asl.ParseOpcode == PARSEOP_INTEGER) &&
            (Op->Asl.Child->Asl.Value.Integer > ACPI_UINT8_MAX))
        {
            AslError (ASL_ERROR, ASL_MSG_INVALID_TIME, Op, NULL);
        }
        break;

    case PARSEOP_DEVICE:

        /* Check usage of _HID and _ADR objects */

        HidExists = ApFindNameInDeviceTree (METHOD_NAME__HID, Op);
        AdrExists = ApFindNameInDeviceTree (METHOD_NAME__ADR, Op);

        if (!HidExists && !AdrExists)
        {
            AslError (ASL_WARNING, ASL_MSG_MISSING_DEPENDENCY, Op,
                "Device object requires a _HID or _ADR in same scope");
        }
        else if (HidExists && AdrExists)
        {
            /*
             * According to the ACPI spec, "A device object must contain
             * either an _HID object or an _ADR object, but should not contain
             * both".
             */
            AslError (ASL_WARNING, ASL_MSG_MULTIPLE_TYPES, Op,
                "Device object requires either a _HID or _ADR, but not both");
        }
        break;

    case PARSEOP_EVENT:
    case PARSEOP_MUTEX:
    case PARSEOP_OPERATIONREGION:
    case PARSEOP_POWERRESOURCE:
    case PARSEOP_PROCESSOR:
    case PARSEOP_THERMALZONE:

        /*
         * The first operand is a name to be created in the namespace.
         * Check against the reserved list.
         */
        i = ApCheckForPredefinedName (Op, Op->Asl.NameSeg);
        if (i < ACPI_VALID_RESERVED_NAME_MAX)
        {
            AslError (ASL_ERROR, ASL_MSG_RESERVED_USE,
                Op, Op->Asl.ExternalName);
        }
        break;

    case PARSEOP_NAME:

        /* Typecheck any predefined names statically defined with Name() */

        ApCheckForPredefinedObject (Op, Op->Asl.NameSeg);

        /* Special typechecking for _HID */

        if (ACPI_COMPARE_NAMESEG (METHOD_NAME__HID, Op->Asl.NameSeg))
        {
            Next = Op->Asl.Child->Asl.Next;
            AnCheckId (Next, ASL_TYPE_HID);
        }

        /* Special typechecking for _CID */

        else if (ACPI_COMPARE_NAMESEG (METHOD_NAME__CID, Op->Asl.NameSeg))
        {
            Next = Op->Asl.Child->Asl.Next;

            if ((Next->Asl.ParseOpcode == PARSEOP_PACKAGE) ||
                (Next->Asl.ParseOpcode == PARSEOP_VAR_PACKAGE))
            {
                Next = Next->Asl.Child;
                while (Next)
                {
                    AnCheckId (Next, ASL_TYPE_CID);
                    Next = Next->Asl.Next;
                }
            }
            else
            {
                AnCheckId (Next, ASL_TYPE_CID);
            }
        }

        break;

    default:

        break;
    }

    /* Check for named object creation within a non-serialized method */

    MtCheckNamedObjectInMethod (Op, MethodInfo);
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    MtCheckNamedObjectInMethod
 *
 * PARAMETERS:  Op                  - Current parser op
 *              MethodInfo          - Info for method being parsed
 *
 * RETURN:      None
 *
 * DESCRIPTION: Detect if a non-serialized method is creating a named object,
 *              which could possibly cause problems if two threads execute
 *              the method concurrently. Emit a remark in this case.
 *
 ******************************************************************************/

static void
MtCheckNamedObjectInMethod (
    ACPI_PARSE_OBJECT       *Op,
    ASL_METHOD_INFO         *MethodInfo)
{
    const ACPI_OPCODE_INFO  *OpInfo;


    /* We don't care about actual method declarations or scopes */

    if ((Op->Asl.AmlOpcode == AML_METHOD_OP) ||
        (Op->Asl.AmlOpcode == AML_SCOPE_OP))
    {
        return;
    }

    /* Determine if we are creating a named object within a method */

    if (!MethodInfo)
    {
        return;
    }

    OpInfo = AcpiPsGetOpcodeInfo (Op->Asl.AmlOpcode);
    if (OpInfo->Class == AML_CLASS_NAMED_OBJECT)
    {
        /*
         * 1) Mark the method as a method that creates named objects.
         *
         * 2) If the method is non-serialized, emit a remark that the method
         * should be serialized.
         *
         * Reason: If a thread blocks within the method for any reason, and
         * another thread enters the method, the method will fail because
         * an attempt will be made to create the same object twice.
         */
        MethodInfo->CreatesNamedObjects = TRUE;
        if (!MethodInfo->ShouldBeSerialized)
        {
            AslError (ASL_REMARK, ASL_MSG_SERIALIZED_REQUIRED, MethodInfo->Op,
                "due to creation of named objects within");

            /* Emit message only ONCE per method */

            MethodInfo->ShouldBeSerialized = TRUE;
        }
    }
}


/*******************************************************************************
 *
 * FUNCTION:    MtMethodAnalysisWalkEnd
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Ascending callback for analysis walk. Complete method
 *              return analysis.
 *
 ******************************************************************************/

ACPI_STATUS
MtMethodAnalysisWalkEnd (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ASL_ANALYSIS_WALK_INFO  *WalkInfo = (ASL_ANALYSIS_WALK_INFO *) Context;
    ASL_METHOD_INFO         *MethodInfo = WalkInfo->MethodStack;


    switch (Op->Asl.ParseOpcode)
    {
    case PARSEOP_METHOD:
    case PARSEOP_RETURN:

        if (!MethodInfo)
        {
            printf ("No method info for method! [%s]\n", Op->Asl.Namepath);
            AslError (ASL_ERROR, ASL_MSG_COMPILER_INTERNAL, Op,
                "No method info for this method");

            CmCleanupAndExit ();
            return (AE_AML_INTERNAL);
        }
        break;

    default:

        break;
    }

    switch (Op->Asl.ParseOpcode)
    {
    case PARSEOP_METHOD:

        WalkInfo->MethodStack = MethodInfo->Next;

        /*
         * Check if there is no return statement at the end of the
         * method AND we can actually get there -- i.e., the execution
         * of the method can possibly terminate without a return statement.
         */
        if ((!AnLastStatementIsReturn (Op)) &&
            (!(Op->Asl.CompileFlags & OP_HAS_NO_EXIT)))
        {
            /*
             * No return statement, and execution can possibly exit
             * via this path. This is equivalent to Return ()
             */
            MethodInfo->NumReturnNoValue++;
        }

        /*
         * Check for case where some return statements have a return value
         * and some do not. Exit without a return statement is a return with
         * no value
         */
        if (MethodInfo->NumReturnNoValue &&
            MethodInfo->NumReturnWithValue)
        {
            AslError (ASL_WARNING, ASL_MSG_RETURN_TYPES, Op,
                Op->Asl.ExternalName);
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

#include "aslcompiler.h"
#include "aslcompiler.y.h"
#include "acnamesp.h"
#include "acparser.h"
#include "amlcode.h"


#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslmethod")


/* Local prototypes */

static void
MtCheckNamedObjectInMethod (
    ACPI_PARSE_OBJECT       *Op,
    ASL_METHOD_INFO         *MethodInfo);

static void
MtCheckStaticOperationRegionInMethod (
    ACPI_PARSE_OBJECT       *Op);


/*******************************************************************************
 *
 * FUNCTION:    MtMethodAnalysisWalkBegin
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Descending callback for the analysis walk. Check methods for:
 *              1) Initialized local variables
 *              2) Valid arguments
 *              3) Return types
 *
 ******************************************************************************/

ACPI_STATUS
MtMethodAnalysisWalkBegin (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ASL_ANALYSIS_WALK_INFO  *WalkInfo = (ASL_ANALYSIS_WALK_INFO *) Context;
    ASL_METHOD_INFO         *MethodInfo = WalkInfo->MethodStack;
    ACPI_PARSE_OBJECT       *Next;
    UINT32                  RegisterNumber;
    UINT32                  i;
    char                    LocalName[] = "Local0";
    char                    ArgName[] = "Arg0";
    ACPI_PARSE_OBJECT       *ArgNode;
    ACPI_PARSE_OBJECT       *NextType;
    UINT8                   ActualArgs = 0;
    BOOLEAN                 HidExists;
    BOOLEAN                 AdrExists;
    BOOLEAN                 PrsExists;
    BOOLEAN                 CrsExists;
    BOOLEAN                 SrsExists;
    BOOLEAN                 DisExists;


    /* Build cross-reference output file if requested */

    if (AslGbl_CrossReferenceOutput)
    {
        OtXrefWalkPart1 (Op, Level, MethodInfo);
    }

    switch (Op->Asl.ParseOpcode)
    {
    case PARSEOP_METHOD:

        AslGbl_TotalMethods++;

        /* Create and init method info */

        MethodInfo = UtLocalCalloc (sizeof (ASL_METHOD_INFO));
        MethodInfo->Next = WalkInfo->MethodStack;
        MethodInfo->Op = Op;

        WalkInfo->MethodStack = MethodInfo;

        /*
         * Special handling for _PSx methods. Dependency rules (same scope):
         *
         * 1) _PS0 - One of these must exist: _PS1, _PS2, _PS3
         * 2) _PS1/_PS2/_PS3: A _PS0 must exist
         */
        if (ACPI_COMPARE_NAMESEG (METHOD_NAME__PS0, Op->Asl.NameSeg))
        {
            /* For _PS0, one of _PS1/_PS2/_PS3 must exist */

            if ((!ApFindNameInScope (METHOD_NAME__PS1, Op)) &&
                (!ApFindNameInScope (METHOD_NAME__PS2, Op)) &&
                (!ApFindNameInScope (METHOD_NAME__PS3, Op)))
            {
                AslError (ASL_WARNING, ASL_MSG_MISSING_DEPENDENCY, Op,
                    "_PS0 requires one of _PS1/_PS2/_PS3 in same scope");
            }
        }
        else if (
            ACPI_COMPARE_NAMESEG (METHOD_NAME__PS1, Op->Asl.NameSeg) ||
            ACPI_COMPARE_NAMESEG (METHOD_NAME__PS2, Op->Asl.NameSeg) ||
            ACPI_COMPARE_NAMESEG (METHOD_NAME__PS3, Op->Asl.NameSeg))
        {
            /* For _PS1/_PS2/_PS3, a _PS0 must exist */

            if (!ApFindNameInScope (METHOD_NAME__PS0, Op))
            {
                sprintf (AslGbl_MsgBuffer,
                    "%4.4s requires _PS0 in same scope", Op->Asl.NameSeg);

                AslError (ASL_WARNING, ASL_MSG_MISSING_DEPENDENCY, Op,
                    AslGbl_MsgBuffer);
            }
        }

        /* Get the name node */

        Next = Op->Asl.Child;

        /* Get the NumArguments node */

        Next = Next->Asl.Next;
        MethodInfo->NumArguments = (UINT8)
            (((UINT8) Next->Asl.Value.Integer) & 0x07);

        /* Get the SerializeRule and SyncLevel nodes, ignored here */

        Next = Next->Asl.Next;
        MethodInfo->ShouldBeSerialized = (UINT8) Next->Asl.Value.Integer;

        Next = Next->Asl.Next;
        ArgNode = Next;

        /* Get the ReturnType node */

        Next = Next->Asl.Next;

        NextType = Next->Asl.Child;

        MethodInfo->ValidReturnTypes = MtProcessTypeOp (NextType);
        Op->Asl.AcpiBtype |= MethodInfo->ValidReturnTypes;

        /* Get the ParameterType node */

        Next = Next->Asl.Next;

        NextType = Next->Asl.Child;
        if (!NextType)
        {
            /*
             * The optional parameter types list was omitted  at the source
             * level. Use the Argument count parameter instead.
             */
            ActualArgs = MethodInfo->NumArguments;
        }
        else
        {
            ActualArgs = MtProcessParameterTypeList (NextType,
                MethodInfo->ValidArgTypes);
            MethodInfo->NumArguments = ActualArgs;
            ArgNode->Asl.Value.Integer |= ActualArgs;
        }

        if ((MethodInfo->NumArguments) &&
            (MethodInfo->NumArguments != ActualArgs))
        {
            sprintf (AslGbl_MsgBuffer,
                "Length = %u", ActualArgs);
            AslError (ASL_ERROR, ASL_MSG_ARG_COUNT_MISMATCH,
                Op->Asl.Child->Asl.Next, AslGbl_MsgBuffer);
        }

        /* Allow numarguments == 0 for Function() */

        if ((!MethodInfo->NumArguments) && (ActualArgs))
        {
            MethodInfo->NumArguments = ActualArgs;
            ArgNode->Asl.Value.Integer |= ActualArgs;
        }

        /*
         * Actual arguments are initialized at method entry.
         * All other ArgX "registers" can be used as locals, so we
         * track their initialization.
         */
        for (i = 0; i < MethodInfo->NumArguments; i++)
        {
            MethodInfo->ArgInitialized[i] = TRUE;
        }
        break;

    case PARSEOP_METHODCALL:

        /* Check for a recursive method call */

        if (MethodInfo &&
           (Op->Asl.Node == MethodInfo->Op->Asl.Node))
        {
            if (MethodInfo->CreatesNamedObjects)
            {
                /*
                 * This is an error, as it will fail at runtime on all ACPI
                 * implementations. Any named object declarations will be
                 * executed twice, causing failure the second time. Note,
                 * this is independent of whether the method is declared
                 * Serialized, because the same thread is attempting to
                 * reenter the method, and this will always succeed.
                 */
                AslDualParseOpError (ASL_ERROR, ASL_MSG_ILLEGAL_RECURSION, Op,
                    Op->Asl.Value.String, ASL_MSG_FOUND_HERE, MethodInfo->Op,
                    MethodInfo->Op->Asl.ExternalName);
            }
            else
            {
                /* Method does not create objects, issue a remark */

                AslError (ASL_REMARK, ASL_MSG_RECURSION, Op, Op->Asl.ExternalName);
            }
        }
        break;

    case PARSEOP_LOCAL0:
    case PARSEOP_LOCAL1:
    case PARSEOP_LOCAL2:
    case PARSEOP_LOCAL3:
    case PARSEOP_LOCAL4:
    case PARSEOP_LOCAL5:
    case PARSEOP_LOCAL6:
    case PARSEOP_LOCAL7:

        if (!MethodInfo)
        {
            /*
             * Local was used outside a control method, or there was an error
             * in the method declaration.
             */
            AslError (ASL_REMARK, ASL_MSG_LOCAL_OUTSIDE_METHOD,
                Op, Op->Asl.ExternalName);
            return (AE_ERROR);
        }

        RegisterNumber = (Op->Asl.AmlOpcode & 0x0007);

        /*
         * If the local is being used as a target, mark the local
         * initialized
         */
        if (Op->Asl.CompileFlags & OP_IS_TARGET)
        {
            MethodInfo->LocalInitialized[RegisterNumber] = TRUE;
        }

        /*
         * Otherwise, this is a reference, check if the local
         * has been previously initialized.
         *
         * The only operator that accepts an uninitialized value is ObjectType()
         */
        else if ((!MethodInfo->LocalInitialized[RegisterNumber]) &&
                 (Op->Asl.Parent->Asl.ParseOpcode != PARSEOP_OBJECTTYPE))
        {
            LocalName[strlen (LocalName) -1] = (char) (RegisterNumber + 0x30);
            AslError (ASL_ERROR, ASL_MSG_LOCAL_INIT, Op, LocalName);
        }
        break;

    case PARSEOP_ARG0:
    case PARSEOP_ARG1:
    case PARSEOP_ARG2:
    case PARSEOP_ARG3:
    case PARSEOP_ARG4:
    case PARSEOP_ARG5:
    case PARSEOP_ARG6:

        if (!MethodInfo)
        {
            /*
             * Arg was used outside a control method, or there was an error
             * in the method declaration.
             */
            AslError (ASL_REMARK, ASL_MSG_LOCAL_OUTSIDE_METHOD,
                Op, Op->Asl.ExternalName);
            return (AE_ERROR);
        }

        RegisterNumber = (Op->Asl.AmlOpcode & 0x000F) - 8;
        ArgName[strlen (ArgName) -1] = (char) (RegisterNumber + 0x30);

        /*
         * If the Arg is being used as a target, mark the local
         * initialized
         */
        if (Op->Asl.CompileFlags & OP_IS_TARGET)
        {
            MethodInfo->ArgInitialized[RegisterNumber] = TRUE;
        }

        /*
         * Otherwise, this is a reference, check if the Arg
         * has been previously initialized.
         *
         * The only operator that accepts an uninitialized value is ObjectType()
         */
        else if ((!MethodInfo->ArgInitialized[RegisterNumber]) &&
            (Op->Asl.Parent->Asl.ParseOpcode != PARSEOP_OBJECTTYPE))
        {
            AslError (ASL_ERROR, ASL_MSG_ARG_INIT, Op, ArgName);
        }

        /* Flag this arg if it is not a "real" argument to the method */

        if (RegisterNumber >= MethodInfo->NumArguments)
        {
            AslError (ASL_REMARK, ASL_MSG_NOT_PARAMETER, Op, ArgName);
        }
        break;

    case PARSEOP_RETURN:

        if (!MethodInfo)
        {
            /*
             * Probably was an error in the method declaration,
             * no additional error here
             */
            ACPI_WARNING ((AE_INFO, "%p, No parent method", Op));
            return (AE_ERROR);
        }

        /*
         * A child indicates a possible return value. A simple Return or
         * Return() is marked with OP_IS_NULL_RETURN by the parser so
         * that it is not counted as a "real" return-with-value, although
         * the AML code that is actually emitted is Return(0). The AML
         * definition of Return has a required parameter, so we are
         * forced to convert a null return to Return(0).
         */
        if ((Op->Asl.Child) &&
            (Op->Asl.Child->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG) &&
            (!(Op->Asl.Child->Asl.CompileFlags & OP_IS_NULL_RETURN)))
        {
            MethodInfo->NumReturnWithValue++;
        }
        else
        {
            MethodInfo->NumReturnNoValue++;
        }
        break;

    case PARSEOP_BREAK:
    case PARSEOP_CONTINUE:

        Next = Op->Asl.Parent;
        while (Next)
        {
            if (Next->Asl.ParseOpcode == PARSEOP_WHILE)
            {
                break;
            }
            Next = Next->Asl.Parent;
        }

        if (!Next)
        {
            AslError (ASL_ERROR, ASL_MSG_NO_WHILE, Op, NULL);
        }
        break;

    case PARSEOP_STALL:

        /* We can range check if the argument is an integer */

        if ((Op->Asl.Child->Asl.ParseOpcode == PARSEOP_INTEGER) &&
            (Op->Asl.Child->Asl.Value.Integer > ACPI_UINT8_MAX))
        {
            AslError (ASL_ERROR, ASL_MSG_INVALID_TIME, Op, NULL);
        }
        break;

    case PARSEOP_DEVICE:

        /* Check usage of _HID and _ADR objects */

        HidExists = ApFindNameInDeviceTree (METHOD_NAME__HID, Op);
        AdrExists = ApFindNameInDeviceTree (METHOD_NAME__ADR, Op);

        if (!HidExists && !AdrExists)
        {
            AslError (ASL_ERROR, ASL_MSG_MISSING_DEPENDENCY, Op,
                "Device object requires a _HID or _ADR");
        }
        else if (HidExists && AdrExists)
        {
            /*
             * According to the ACPI spec, "A device object must contain
             * either an _HID object or an _ADR object, but should not contain
             * both".
             */
            AslError (ASL_WARNING, ASL_MSG_MULTIPLE_TYPES, Op,
                "Device object requires either a _HID or _ADR, but not both");
        }

        /*
         * Check usage of _CRS, _DIS, _PRS, and _SRS objects (July 2021).
         *
         * Under the Device Object:
         *
         * 1) If _PRS present, must have _CRS and _SRS
         * 2) If _SRS present, must have _PRS (_PRS requires _CRS and _SRS)
         * 3) If _DIS present, must have _SRS (_SRS requires _PRS, _PRS requires _CRS and _SRS)
         * 4) If _SRS present, probably should have a _DIS (Remark only)
         */
        CrsExists = ApFindNameInDeviceTree (METHOD_NAME__CRS, Op);
        DisExists = ApFindNameInDeviceTree (METHOD_NAME__DIS, Op);
        PrsExists = ApFindNameInDeviceTree (METHOD_NAME__PRS, Op);
        SrsExists = ApFindNameInDeviceTree (METHOD_NAME__SRS, Op);

        /* 1) If _PRS is present, must have a _CRS and _SRS */

        if (PrsExists)
        {
            if (!CrsExists)
            {
                AslError (ASL_WARNING, ASL_MSG_MISSING_DEPENDENCY, Op,
                    "Device has a _PRS, missing a _CRS, required");
            }
            if (!SrsExists)
            {
                AslError (ASL_WARNING, ASL_MSG_MISSING_DEPENDENCY, Op,
                    "Device has a _PRS, missing a _SRS, required");
            }
        }

        /* 2) If _SRS is present, must have _PRS (_PRS requires _CRS and _SRS) */

        if ((SrsExists) && (!PrsExists))
        {
            AslError (ASL_WARNING, ASL_MSG_MISSING_DEPENDENCY, Op,
                "Device has a _SRS, missing a _PRS, required");
        }

        /* 3) If _DIS is present, must have a _SRS */

        if ((DisExists) && (!SrsExists))
        {
            AslError (ASL_WARNING, ASL_MSG_MISSING_DEPENDENCY, Op,
                "Device has a _DIS, missing a _SRS, required");
        }

        /*
         * 4) If _SRS is present, should have a _DIS (_PRS requires _CRS
         * and _SRS)  Remark only.
         */
        if ((SrsExists) && (!DisExists))
        {
            AslError (ASL_REMARK, ASL_MSG_MISSING_DEPENDENCY, Op,
                "Device has a _SRS, no corresponding _DIS");
        }
        break;

    case PARSEOP_EVENT:
    case PARSEOP_MUTEX:
    case PARSEOP_OPERATIONREGION:
    case PARSEOP_POWERRESOURCE:
    case PARSEOP_PROCESSOR:
    case PARSEOP_THERMALZONE:

        /*
         * The first operand is a name to be created in the namespace.
         * Check against the reserved list.
         */
        i = ApCheckForPredefinedName (Op, Op->Asl.NameSeg);
        if (i < ACPI_VALID_RESERVED_NAME_MAX)
        {
            AslError (ASL_ERROR, ASL_MSG_RESERVED_USE,
                Op, Op->Asl.ExternalName);
        }

        MtCheckStaticOperationRegionInMethod (Op);
        break;

    case PARSEOP_NAME:

        /* Typecheck any predefined names statically defined with Name() */

        ApCheckForPredefinedObject (Op, Op->Asl.NameSeg);

        /* Special typechecking for _HID */

        if (ACPI_COMPARE_NAMESEG (METHOD_NAME__HID, Op->Asl.NameSeg))
        {
            Next = Op->Asl.Child->Asl.Next;
            AnCheckId (Next, ASL_TYPE_HID);
        }

        /* Special typechecking for _CID */

        else if (ACPI_COMPARE_NAMESEG (METHOD_NAME__CID, Op->Asl.NameSeg))
        {
            Next = Op->Asl.Child->Asl.Next;

            if ((Next->Asl.ParseOpcode == PARSEOP_PACKAGE) ||
                (Next->Asl.ParseOpcode == PARSEOP_VAR_PACKAGE))
            {
                Next = Next->Asl.Child;
                while (Next)
                {
                    AnCheckId (Next, ASL_TYPE_CID);
                    Next = Next->Asl.Next;
                }
            }
            else
            {
                AnCheckId (Next, ASL_TYPE_CID);
            }
        }

        break;

    default:

        break;
    }

    /* Check for named object creation within a non-serialized method */

    MtCheckNamedObjectInMethod (Op, MethodInfo);
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    MtProcessTypeOp
 *
 * PARAMETERS:  Op                  - Op representing a btype
 *
 * RETURN:      Btype represented by Op
 *
 * DESCRIPTION: Process a parse object that represents single parameter type or
 *              a return type in method, function, and external declarations.
 *
 ******************************************************************************/

UINT32
MtProcessTypeOp (
    ACPI_PARSE_OBJECT       *TypeOp)
{
    UINT32                  Btype = ACPI_BTYPE_ANY;


    while (TypeOp)
    {
        Btype |= AnMapObjTypeToBtype (TypeOp);
        TypeOp->Asl.ParseOpcode = PARSEOP_DEFAULT_ARG;
        TypeOp = TypeOp->Asl.Next;
    }

    return (Btype);
}


/*******************************************************************************
 *
 * FUNCTION:    MtProcessParameterTypeList
 *
 * PARAMETERS:  Op                  - Op representing a btype
 *
 * RETURN:      Btype represented by Op
 *
 * DESCRIPTION: Process a parse object that represents a parameter type list in
 *              method, function, and external declarations.
 *
 ******************************************************************************/

UINT8
MtProcessParameterTypeList (
    ACPI_PARSE_OBJECT       *ParamTypeOp,
    UINT32                  *TypeList)
{
    UINT8                   ParameterCount = 0;


    if (ParamTypeOp && ParamTypeOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG)
    {
        /* Special case for a single parameter without braces */

        TypeList[ParameterCount] =
            MtProcessTypeOp (ParamTypeOp);

        return (1);
    }

    while (ParamTypeOp)
    {
        TypeList[ParameterCount] =
            MtProcessTypeOp (ParamTypeOp->Asl.Child);

        ParameterCount++;
        ParamTypeOp = ParamTypeOp->Asl.Next;
    }

    return (ParameterCount);
}


/*******************************************************************************
 *
 * FUNCTION:    MtCheckNamedObjectInMethod
 *
 * PARAMETERS:  Op                  - Current parser op
 *              MethodInfo          - Info for method being parsed
 *
 * RETURN:      None
 *
 * DESCRIPTION: Detect if a non-serialized method is creating a named object,
 *              which could possibly cause problems if two threads execute
 *              the method concurrently. Emit a remark in this case.
 *
 ******************************************************************************/

static void
MtCheckNamedObjectInMethod (
    ACPI_PARSE_OBJECT       *Op,
    ASL_METHOD_INFO         *MethodInfo)
{
    const ACPI_OPCODE_INFO  *OpInfo;
    char                    *ExternalPath;


    /* We don't care about actual method declarations or scopes */

    if ((Op->Asl.AmlOpcode == AML_METHOD_OP) ||
        (Op->Asl.AmlOpcode == AML_SCOPE_OP))
    {
        return;
    }

    /* Determine if we are creating a named object within a method */

    if (!MethodInfo)
    {
        return;
    }

    OpInfo = AcpiPsGetOpcodeInfo (Op->Asl.AmlOpcode);
    if ((OpInfo->Class == AML_CLASS_NAMED_OBJECT) && (Op->Asl.AmlOpcode != AML_FIELD_OP))
    {
        /*
         * 1) Mark the method as a method that creates named objects.
         *
         * 2) Issue a remark indicating the inefficiency of creating named
         * objects within a method (Except for compiler-emitted temporary
         * variables).
         *
         * 3) If the method is non-serialized, emit a remark that the method
         * should be serialized.
         *
         * Reason: If a thread blocks within the method for any reason, and
         * another thread enters the method, the method will fail because
         * an attempt will be made to create the same object twice.
         *
         * Note: The Field opcode is disallowed here because Field() does not
         * create a new named object.
         */
        ExternalPath = AcpiNsGetNormalizedPathname (MethodInfo->Op->Asl.Node, TRUE);

        /* No error for compiler temp variables (name starts with "_T_") */

        if ((Op->Asl.NameSeg[0] != '_') &&
            (Op->Asl.NameSeg[1] != 'T') &&
            (Op->Asl.NameSeg[2] != '_'))
        {
            AslError (ASL_REMARK, ASL_MSG_NAMED_OBJECT_CREATION, Op,
                ExternalPath);
        }

        MethodInfo->CreatesNamedObjects = TRUE;
        if (!MethodInfo->ShouldBeSerialized)
        {
            AslError (ASL_REMARK, ASL_MSG_SERIALIZED_REQUIRED, MethodInfo->Op,
                ExternalPath);

            /* Emit message only ONCE per method */

            MethodInfo->ShouldBeSerialized = TRUE;
        }

        if (ExternalPath)
        {
            ACPI_FREE (ExternalPath);
        }
    }
}


/*******************************************************************************
 *
 * FUNCTION:    MtCheckStaticOperationRegionInMethod
 *
 * PARAMETERS:  Op                  - Current parser op
 *
 * RETURN:      None
 *
 * DESCRIPTION: Warns if an Operation Region with static address or length
 *              is declared inside a control method
 *
 ******************************************************************************/

static void
MtCheckStaticOperationRegionInMethod(
    ACPI_PARSE_OBJECT*       Op)
{
    ACPI_PARSE_OBJECT*       AddressOp;
    ACPI_PARSE_OBJECT*       LengthOp;


    if (Op->Asl.ParseOpcode != PARSEOP_OPERATIONREGION)
    {
        return;
    }

    /*
     * OperationRegion should have 4 arguments defined. At this point, we
     * assume that the parse tree is well-formed.
     */
    AddressOp = Op->Asl.Child->Asl.Next->Asl.Next;
    LengthOp = Op->Asl.Child->Asl.Next->Asl.Next->Asl.Next;

    if (UtGetParentMethodOp (Op) &&
        AddressOp->Asl.ParseOpcode == PARSEOP_INTEGER &&
        LengthOp->Asl.ParseOpcode == PARSEOP_INTEGER)
    {
        /*
         * At this point, a static operation region declared inside of a
         * control method has been found. Throw a warning because this is
         * highly inefficient.
         */
        AslError(ASL_WARNING, ASL_MSG_STATIC_OPREGION_IN_METHOD, Op, NULL);
    }

    return;
}


/*******************************************************************************
 *
 * FUNCTION:    MtMethodAnalysisWalkEnd
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Ascending callback for analysis walk. Complete method
 *              return analysis.
 *
 ******************************************************************************/

ACPI_STATUS
MtMethodAnalysisWalkEnd (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ASL_ANALYSIS_WALK_INFO  *WalkInfo = (ASL_ANALYSIS_WALK_INFO *) Context;
    ASL_METHOD_INFO         *MethodInfo = WalkInfo->MethodStack;
    char                    *ExternalPath;


    switch (Op->Asl.ParseOpcode)
    {
    case PARSEOP_METHOD:
    case PARSEOP_RETURN:

        if (!MethodInfo)
        {
            printf ("No method info for method! [%s]\n", Op->Asl.Namepath);
            AslError (ASL_ERROR, ASL_MSG_COMPILER_INTERNAL, Op,
                "No method info for this method");

            CmCleanupAndExit ();
            return (AE_AML_INTERNAL);
        }
        break;

    default:

        break;
    }

    switch (Op->Asl.ParseOpcode)
    {
    case PARSEOP_METHOD:

        WalkInfo->MethodStack = MethodInfo->Next;

        /*
         * Check if there is no return statement at the end of the
         * method AND we can actually get there -- i.e., the execution
         * of the method can possibly terminate without a return statement.
         */
        if ((!AnLastStatementIsReturn (Op)) &&
            (!(Op->Asl.CompileFlags & OP_HAS_NO_EXIT)))
        {
            /*
             * No return statement, and execution can possibly exit
             * via this path. This is equivalent to Return ()
             */
            MethodInfo->NumReturnNoValue++;
        }

        /*
         * Check for case where some return statements have a return value
         * and some do not. Exit without a return statement is a return with
         * no value
         */
        if (MethodInfo->NumReturnNoValue &&
            MethodInfo->NumReturnWithValue)
        {
            ExternalPath = AcpiNsGetNormalizedPathname (Op->Asl.Node, TRUE);

            AslError (ASL_WARNING, ASL_MSG_RETURN_TYPES, Op,
                ExternalPath);

            if (ExternalPath)
            {
                ACPI_FREE (ExternalPath);
            }
>>>>>>> BRANCH (a8f750 Project import generated by Copybara.)
        }

        /*
         * If there are any RETURN() statements with no value, or there is a
         * control path that allows the method to exit without a return value,
         * we mark the method as a method that does not return a value. This
         * knowledge can be used to check method invocations that expect a
         * returned value.
         */
        if (MethodInfo->NumReturnNoValue)
        {
            if (MethodInfo->NumReturnWithValue)
            {
                Op->Asl.CompileFlags |= OP_METHOD_SOME_NO_RETVAL;
            }
            else
            {
                Op->Asl.CompileFlags |= OP_METHOD_NO_RETVAL;
            }
        }

        /*
         * Check predefined method names for correct return behavior
         * and correct number of arguments. Also, some special checks
         * For GPE and _REG methods.
         */
        if (ApCheckForPredefinedMethod (Op, MethodInfo))
        {
            /* Special check for two names like _L01 and _E01 in same scope */

            ApCheckForGpeNameConflict (Op);

            /*
             * Special check for _REG: Must have an operation region definition
             * within the same scope!
             */
            ApCheckRegMethod (Op);
        }

        ACPI_FREE (MethodInfo);
        break;

    case PARSEOP_NAME:

         /* Special check for two names like _L01 and _E01 in same scope */

        ApCheckForGpeNameConflict (Op);
        break;

    case PARSEOP_RETURN:

        /*
         * If the parent is a predefined method name, attempt to typecheck
         * the return value. Only static types can be validated.
         */
        ApCheckPredefinedReturnValue (Op, MethodInfo);

        /*
         * The parent block does not "exit" and continue execution -- the
         * method is terminated here with the Return() statement.
         */
        Op->Asl.Parent->Asl.CompileFlags |= OP_HAS_NO_EXIT;

        /* Used in the "typing" pass later */

        Op->Asl.ParentMethod = MethodInfo->Op;

        /*
         * If there is a peer node after the return statement, then this
         * node is unreachable code -- i.e., it won't be executed because of
         * the preceding Return() statement.
         */
        if (Op->Asl.Next)
        {
            AslError (ASL_WARNING, ASL_MSG_UNREACHABLE_CODE,
                Op->Asl.Next, NULL);
        }
        break;

    case PARSEOP_IF:

        if ((Op->Asl.CompileFlags & OP_HAS_NO_EXIT) &&
            (Op->Asl.Next) &&
            (Op->Asl.Next->Asl.ParseOpcode == PARSEOP_ELSE))
        {
            /*
             * This IF has a corresponding ELSE. The IF block has no exit,
             * (it contains an unconditional Return)
             * mark the ELSE block to remember this fact.
             */
            Op->Asl.Next->Asl.CompileFlags |= OP_IF_HAS_NO_EXIT;
        }
        break;

    case PARSEOP_ELSE:

        if ((Op->Asl.CompileFlags & OP_HAS_NO_EXIT) &&
            (Op->Asl.CompileFlags & OP_IF_HAS_NO_EXIT))
        {
            /*
             * This ELSE block has no exit and the corresponding IF block
             * has no exit either. Therefore, the parent node has no exit.
             */
            Op->Asl.Parent->Asl.CompileFlags |= OP_HAS_NO_EXIT;
        }
        break;


    default:

        if ((Op->Asl.CompileFlags & OP_HAS_NO_EXIT) &&
            (Op->Asl.Parent))
        {
            /* If this node has no exit, then the parent has no exit either */

            Op->Asl.Parent->Asl.CompileFlags |= OP_HAS_NO_EXIT;
        }
        break;
    }

    return (AE_OK);
}
