/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "codegen/ARM64ConditionCode.hpp"
#include "codegen/ARM64Instruction.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"

TR::Register *
genericReturnEvaluator(TR::Node *node, TR::RealRegister::RegNum rnum, TR_RegisterKinds rk, TR_ReturnInfo i,  TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *returnRegister = cg->evaluate(firstChild);

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(1, 0, cg->trMemory());
   deps->addPreCondition(returnRegister, rnum);
   generateAdminInstruction(cg, TR::InstOpCode::retn, node, deps);

   cg->comp()->setReturnInfo(i);
   cg->decReferenceCount(firstChild);

   return NULL;
   }

// also handles iureturn
TR::Register *
OMR::ARM64::TreeEvaluator::ireturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return genericReturnEvaluator(node, cg->getProperties().getIntegerReturnRegister(), TR_GPR, TR_IntReturn, cg);
   }

// also handles lureturn, areturn
TR::Register *
OMR::ARM64::TreeEvaluator::lreturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return genericReturnEvaluator(node, cg->getProperties().getLongReturnRegister(), TR_GPR, TR_LongReturn, cg);
   }

// void return
TR::Register *
OMR::ARM64::TreeEvaluator::returnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   generateAdminInstruction(cg, TR::InstOpCode::retn, node);
   cg->comp()->setReturnInfo(TR_VoidReturn);
   return NULL;
   }

static TR::Instruction *ificmpHelper(TR::Node *node, TR::ARM64ConditionCode cc, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Node *thirdChild = NULL;
   TR::Register *src1Reg = cg->evaluate(firstChild);
   bool useRegCompare = true;

   if (secondChild->getOpCode().isLoadConst() && secondChild->getRegister() == NULL)
      {
      int32_t value = secondChild->getInt();
      if (constantIsUnsignedImm12(value))
         {
         generateCompareImmInstruction(cg, node, src1Reg, value, false);
         useRegCompare = false;
         }
      }

   if (useRegCompare)
      {
      TR::Register *src2Reg = cg->evaluate(secondChild);
      generateCompareInstruction(cg, node, src1Reg, src2Reg, false);
      }

   TR::LabelSymbol *dstLabel = node->getBranchDestination()->getNode()->getLabel();
   TR::Instruction *result;
   if (node->getNumChildren() == 3)
      {
      thirdChild = node->getChild(2);
      TR_ASSERT(thirdChild->getOpCodeValue() == TR::GlRegDeps, "The third child of a compare must be a TR::GlRegDeps");

      cg->evaluate(thirdChild);

      TR::RegisterDependencyConditions *deps = generateRegisterDependencyConditions(cg, thirdChild, 0);
      result = generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, dstLabel, cc, deps);
      }
   else
      {
      result = generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, dstLabel, cc);
      }

   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   if (thirdChild)
      {
      thirdChild->decReferenceCount();
      }
   return result;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::ificmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(node, TR::CC_EQ, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::ificmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(node, TR::CC_NE, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::ificmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(node, TR::CC_LT, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::ificmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(node, TR::CC_GE, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::ificmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(node, TR::CC_GT, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::ificmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(node, TR::CC_LE, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::ifiucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::ifiucmpltEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::ifiucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::ifiucmpgeEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::ifiucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::ifiucmpgtEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::ifiucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::ifiucmpleEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::iflcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::iflcmpeqEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::iflcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::iflcmpltEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::iflcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::iflcmpgeEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::iflucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::iflucmpltEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::iflucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::iflucmpgeEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::iflcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::iflcmpgtEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::iflcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::iflcmpleEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::iflucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::iflucmpgtEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::iflucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::iflucmpleEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::ifacmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::ifacmpeqEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

static TR::Register *icmpHelper(TR::Node *node, TR::ARM64ConditionCode cc, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg = cg->allocateRegister();
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   bool useRegCompare = true;

   if (secondChild->getOpCode().isLoadConst() && secondChild->getRegister() == NULL)
      {
      int32_t value = secondChild->getInt();
      if (constantIsUnsignedImm12(value))
         {
         generateCompareImmInstruction(cg, node, src1Reg, value);
         useRegCompare = false;
         }
      }

   if (useRegCompare)
      {
      TR::Register *src2Reg = cg->evaluate(secondChild);
      generateCompareInstruction(cg, node, src1Reg, src2Reg);
      }

   generateCSetInstruction(cg, node, trgReg, cc);

   node->setRegister(trgReg);
   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   return trgReg;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::icmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, TR::CC_EQ, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::icmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, TR::CC_NE, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::icmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, TR::CC_LT, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::icmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, TR::CC_LE, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::icmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, TR::CC_GE, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::icmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, TR::CC_GT, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::iucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, TR::CC_CC, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::iucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, TR::CC_LS, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::iucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, TR::CC_CS, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::iucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, TR::CC_HI, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::lcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::lcmpeqEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::lcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::lcmpneEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::lcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::lcmpltEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::lcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::lcmpgeEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::lcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::lcmpgtEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::lcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::lcmpleEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::lucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::lucmpltEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::lucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::lucmpgeEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::lucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::lucmpgtEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::lucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::lucmpleEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::lcmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::lcmpEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::acmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::acmpeqEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::lookupEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::lookupEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::tableEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::tableEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::NULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::NULLCHKEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::ZEROCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::ZEROCHKEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::ResolveAndNULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::ResolveAndNULLCHKEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::DIVCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::DIVCHKEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::BNDCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::BNDCHKEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::ArrayCopyBNDCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::ArrayCopyBNDCHKEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::ArrayStoreCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::ArrayStoreCHKEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::ArrayCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::ArrayCHKEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}
