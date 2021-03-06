#include "stdafx.h"
#include "Residual.h"
#include "Macroblock.h"
#include "Macroblock_Defines.h"
#include "PicParamSet.h"
#include "CAVLC_Defines.h"

using namespace std;

CResidual::CResidual(UINT8 *pSODB, UINT32 offset, CMacroblock *mb)
{
	m_macroblock_belongs = mb;
	m_pSODB = pSODB;
	m_bypeOffset = offset / 8;
	m_bitOffset = offset % 8;
}

CResidual::~CResidual()
{
}

int CResidual::Parse_macroblock_residual(UINT32 &dataLength)
{
	UINT8 cbp_luma = m_macroblock_belongs->m_cbp_luma;
	UINT8 cbp_chroma = m_macroblock_belongs->m_cbp_chroma;
	UINT32 originOffset = 8 * m_bypeOffset + m_bitOffset;

	if (cbp_luma)
	{
		parse_luma_residual(cbp_luma);

		if (m_macroblock_belongs->m_mb_type == I4MB)
		{
			Dump_residual_info_4x4();
		}
	}

	if (cbp_chroma)
	{
		parse_chroma_residual(cbp_chroma);
		Dump_residual_chroma_DC();
	}

	dataLength = 8 * m_bypeOffset + m_bitOffset - originOffset;
	return kPARSING_ERROR_NO_ERROR;
}

UINT8 CResidual::Get_sub_block_number_coeffs(int block_idc_x, int block_idc_y)
{	
	return luma_residual[block_idc_y][block_idc_x].numCoeff;
}

UINT8 CResidual::Get_sub_block_number_coeffs_chroma(int component, int block_idc_x, int block_idc_y)
{
	return chroma_AC_residual[component][block_idc_y][block_idc_x].numCoeff;
}

void CResidual::Dump_residual_info_4x4()
{
#if TRACE_CONFIG_LOGOUT

#if TRACE_CONFIG_MACROBLOCK_RESIDUAL

	g_traceFile << "Luma Residual 4x4:" << endl;
	for (int outRow = 0; outRow < 4; outRow += 2)
	{
		for (int outColumn = 0; outColumn < 4; outColumn += 2)
		{
			for (int row = outRow; row < outRow + 2; row++)
			{
				for (int column = outColumn; column < outColumn + 2; column++)
				{
					g_traceFile << "Luma[" << row << "][" << column << "]: ";
					if (luma_residual[row][column].emptyBlock)
					{
						g_traceFile << "Empty." << endl;
					}
					else
					{
						g_traceFile << "numCoeff: " << to_string(luma_residual[row][column].numCoeff) << "\ttrailingOnes: " << to_string(luma_residual[row][column].trailingOnes) << endl;
						if (luma_residual[row][column].numCoeff)
						{
							if (luma_residual[row][column].trailingOnes)
							{
								g_traceFile << "\ttrailingSign: ";
								for (int idx = 0; idx < luma_residual[row][column].trailingOnes; idx++)
								{
									g_traceFile << to_string(luma_residual[row][column].trailingSign[idx]) << " ";
								}
								g_traceFile << endl;
							}

							int levelCnt = luma_residual[row][column].numCoeff - luma_residual[row][column].trailingOnes;
							if (levelCnt)
							{
								g_traceFile << "\tlevels: ";
								for (int idx = 0; idx < levelCnt; idx++)
								{
									g_traceFile << to_string(luma_residual[row][column].levels[idx]) << " ";
								}
								g_traceFile << endl;
							}
						}
					}
				}
			}			
		}
	}

#endif

#endif
}

void CResidual::Dump_residual_chroma_DC()
{
#if TRACE_CONFIG_LOGOUT

#if TRACE_CONFIG_MACROBLOCK_RESIDUAL

	g_traceFile << "Chroma Residual DC:" << endl;
	for (int chromaIdx = 0; chromaIdx < 2; chromaIdx++)
	{
		g_traceFile << "ChromaDC[" << chromaIdx << "]: ";
		if (chroma_DC_residual[chromaIdx].emptyBlock)
		{
			g_traceFile << "Empty." << endl;
		} 
		else
		{
			g_traceFile << "numCoeff: " << to_string(chroma_DC_residual[chromaIdx].numCoeff) << "\ttrailingOnes: " << to_string(chroma_DC_residual[chromaIdx].trailingOnes) << endl;
			if (chroma_DC_residual[chromaIdx].numCoeff)
			{
				if (chroma_DC_residual[chromaIdx].trailingOnes)
				{
					g_traceFile << "\ttrailingSign: ";
					for (int idx = 0; idx < chroma_DC_residual[chromaIdx].trailingOnes; idx++)
					{
						g_traceFile << to_string(chroma_DC_residual[chromaIdx].trailingSign[idx]) << " ";
					}
					g_traceFile << endl;
				}

				int levelCnt = chroma_DC_residual[chromaIdx].numCoeff - chroma_DC_residual[chromaIdx].trailingOnes;
				if (levelCnt)
				{
					g_traceFile << "\tlevels: ";
					for (int idx = 0; idx < levelCnt; idx++)
					{
						g_traceFile << to_string(chroma_DC_residual[chromaIdx].levels[idx]) << " ";
					}
					g_traceFile << endl;
				}
			}
		}
	}
#endif

#endif
}

int CResidual::parse_luma_residual(UINT8 cbp_luma)
{
	int err = 0;
	int idx8x8 = 0, block_x = 0, block_y = 0, block_sub_idc_x = 0, block_sub_idc_y = 0;

	for (block_y = 0; block_y < 4; block_y += 2)
	{
		for (block_x = 0; block_x < 4; block_x += 2)
		{
			// 16x16 -> 4 * 8x8						
			if (m_macroblock_belongs->Get_pps_active()->Get_entropy_coding_flag() == false)
			{
				// CAVLC
				for (block_sub_idc_y = block_y; block_sub_idc_y < block_y + 2; block_sub_idc_y++)
				{
					for (block_sub_idc_x = block_x; block_sub_idc_x < block_x + 2; block_sub_idc_x++)
					{
						// 8x8 -> 4 * 4x4
						idx8x8 = 2 * (block_y / 2) + block_x / 2;
						if (cbp_luma & (1 << idx8x8))
						{
							luma_residual[block_sub_idc_y][block_sub_idc_x].emptyBlock = false;
							err = get_luma4x4_coeffs(block_sub_idc_x, block_sub_idc_y);
							if (err < 0)
							{
								return err;
							}
						} 
						else
						{
							luma_residual[block_sub_idc_y][block_sub_idc_x].emptyBlock = true;
						}
					}
				}
			}
			else
			{
				// CABAC
			}
		}
	}

	return kPARSING_ERROR_NO_ERROR;
}

int CResidual::get_luma4x4_coeffs(int block_idc_x, int block_idc_y)
{
	int err = 0;
	int mb_type = m_macroblock_belongs->m_mb_type;
	int block_type = (mb_type == I16MB || mb_type == IPCM) ? LUMA_INTRA16x16AC : LUMA;
	int max_coeff_num = 0;
	int numCoeff_vlcIdx = 0, prefixLength = 0, suffixLength = 0, level_prefix = 0, level_suffix = 0;
	int levelSuffixSize = 0, levelCode = 0, i = 0;
	
	switch (block_type)
	{
	case LUMA:
		max_coeff_num = 16;
		break;
	case LUMA_INTRA16x16DC:
		max_coeff_num = 15;
		break;
	case LUMA_INTRA16x16AC:
		max_coeff_num = 15;
		break;
	default:
		break;
	}

	int numberCurrent = m_macroblock_belongs->Get_number_current(block_idc_x, block_idc_y);
	if (numberCurrent < 2)
	{
		numCoeff_vlcIdx = 0;
	}
	else if (numberCurrent < 4)
	{
		numCoeff_vlcIdx = 1;
	}
	else if (numCoeff_vlcIdx < 8)
	{
		numCoeff_vlcIdx = 2;
	}
	else
	{
		numCoeff_vlcIdx = 3;
	}

	// NumCoeff & TrailingOnes...
	UINT8 numCoeff = 0, trailingOnes = 0;
	int token = 0;
	err = get_numCoeff_and_trailingOnes(numCoeff, trailingOnes, token, numCoeff_vlcIdx);
	if (err < 0)
	{
		return err;
	}
	else
	{
		luma_residual[block_idc_y][block_idc_x].coeffToken = token;
		luma_residual[block_idc_y][block_idc_x].numCoeff = numCoeff;
		luma_residual[block_idc_y][block_idc_x].trailingOnes = trailingOnes;
	}

	if (numCoeff) //包含非0系数
	{
		if (trailingOnes) //拖尾系数
		{
			//读取拖尾系数符号
			int signValue = Get_uint_code_num(m_pSODB, m_bypeOffset, m_bitOffset, trailingOnes);
			int trailingCnt = trailingOnes;
			for (int coeffIdx = 0; coeffIdx < trailingOnes; coeffIdx++)
			{
				trailingCnt--;
				if ((signValue >> trailingCnt) & 1)
				{
					luma_residual[block_idc_y][block_idc_x].trailingSign[coeffIdx] = -1;
				}
				else
				{
					luma_residual[block_idc_y][block_idc_x].trailingSign[coeffIdx] = 1;
				}
			}
		}

		//读取解析level值
		int level = 0;
		if (numCoeff > 10 && trailingOnes < 3)
		{
			//根据上下文初始化suffixLength
			suffixLength = 1;
		}
		for (int k = 0; k <= numCoeff - 1 - trailingOnes; k++)
		{
			err = get_coeff_level(level, k, trailingOnes, suffixLength);
			if (err < 0)
			{
				return err;
			}

			if (suffixLength == 0)
			{
				suffixLength = 1;
			}

			if ((abs(level) >(3 << (suffixLength - 1))) && (suffixLength < 6))
			{
				suffixLength++;
			}

			luma_residual[block_idc_y][block_idc_x].levels[k] = level;
		}

		// 读取解析run
		UINT8 zerosLeft = 0, totalZeros = 0, run = 0;
		if (numCoeff < max_coeff_num)
		{
			err = get_total_zeros(totalZeros, numCoeff - 1);
			if (err < 0)
			{
				return err;
			}
		}
		else
		{
			totalZeros = 0;
		}
		luma_residual[block_idc_y][block_idc_x].totalZeros = totalZeros;

		//读取解析run_before
		int runBefore_vlcIdx = 0;
		i = numCoeff - 1;
		zerosLeft = totalZeros;
		if (zerosLeft > 0 && i > 0)
		{
			do
			{
				runBefore_vlcIdx = (zerosLeft - 1 < 6 ? zerosLeft - 1 : 6);
				err = get_run_before(run, runBefore_vlcIdx);
				if (err < 0)
				{
					return err;
				}
				luma_residual[block_idc_y][block_idc_x].runBefore[i] = run;
				zerosLeft -= run;
				i--;
			} while (zerosLeft != 0 && i != 0);			
		}
		else
		{
			run = 0;
		}
		luma_residual[block_idc_y][block_idc_x].runBefore[i] = zerosLeft;
	}

	return kPARSING_ERROR_NO_ERROR;
}

int CResidual::parse_chroma_residual(UINT8 cbp_chroma)
{
	int err = 0;
	// chroma DC
	for (int idx = 0; idx < 2; idx++)
	{
		if (m_macroblock_belongs->Get_pps_active()->Get_transform_8x8_mode_flag() == false)
		{
			// CAVLC
			err = get_chroma_DC_coeffs(idx);
			if (err < 0)
			{
				return err;
			}
		}
		else
		{
			// CABAC
		}
	}

	if (cbp_chroma & 2)
	{
		int err = 0;
		for (int component = 0; component < 2; component++)
		{
			for (int block_idx_y = 0; block_idx_y  < 2; block_idx_y++)
			{
				for (int block_idx_x = 0; block_idx_x < 2; block_idx_x++)
				{
					err = get_chroma_AC_coeffs(component, block_idx_x, block_idx_y);
					if (err < 0)
					{
						return err;
					}
				}
			}
		}
	}

	return kPARSING_ERROR_NO_ERROR;
}

int CResidual::get_chroma_DC_coeffs(int chroma_idx)
{
	int err = 0;
	int max_coeff_num = 4, token = 0;
	int suffixLength = 0;
	
	UINT8 numCoeff = 0, trailingOnes = 0;
	err = get_numCoeff_and_trailingOnes_chromaDC(numCoeff, trailingOnes, token);
	if (err < 0)
	{
		return err;
	}
	else
	{
		chroma_DC_residual[chroma_idx].coeffToken = token;
		chroma_DC_residual[chroma_idx].numCoeff = numCoeff;
		chroma_DC_residual[chroma_idx].trailingOnes = trailingOnes;
	}

	if (numCoeff) //包含非0系数
	{
		if (trailingOnes) //拖尾系数
		{
			//读取拖尾系数符号
			int signValue = Get_uint_code_num(m_pSODB, m_bypeOffset, m_bitOffset, trailingOnes);
			int trailingCnt = trailingOnes;
			for (int coeffIdx = 0; coeffIdx < trailingOnes; coeffIdx++)
			{
				trailingCnt--;
				if ((signValue >> trailingCnt) & 1)
				{
					chroma_DC_residual[chroma_idx].trailingSign[coeffIdx] = -1;
				}
				else
				{
					chroma_DC_residual[chroma_idx].trailingSign[coeffIdx] = 1;
				}
			}
		}

		//读取解析level值
		int level = 0;
		if (numCoeff > 10 && trailingOnes < 3)
		{
			//根据上下文初始化suffixLength
			suffixLength = 1;
		}
		for (int k = 0; k <= numCoeff - 1 - trailingOnes; k++)
		{
			err = get_coeff_level(level, k, trailingOnes, suffixLength);
			if (err < 0)
			{
				return err;
			}

			if (suffixLength == 0)
			{
				suffixLength = 1;
			}

			if ((abs(level) >(3 << (suffixLength - 1))) && (suffixLength < 6))
			{
				suffixLength++;
			}

			chroma_DC_residual[chroma_idx].levels[k] = level;
		}


		// 读取解析run
		UINT8 zerosLeft = 0, totalZeros = 0, run = 0;
		if (numCoeff < max_coeff_num)
		{
			err = get_total_zeros_chromaDC(totalZeros, numCoeff - 1);
			if (err < 0)
			{
				return err;
			}
		}
		else
		{
			totalZeros = 0;
		}
		chroma_DC_residual[chroma_idx].totalZeros = totalZeros;

		//读取解析run_before
		int runBefore_vlcIdx = 0;
		int i = numCoeff - 1;
		zerosLeft = totalZeros;
		if (zerosLeft > 0 && i > 0)
		{
			do
			{
				runBefore_vlcIdx = (zerosLeft - 1 < 6 ? zerosLeft - 1 : 6);
				err = get_run_before(run, runBefore_vlcIdx);
				if (err < 0)
				{
					return err;
				}
				chroma_DC_residual[chroma_idx].runBefore[i] = run;
				zerosLeft -= run;
				i--;
			} while (zerosLeft != 0 && i != 0);
		}
		else
		{
			run = 0;
		}
		chroma_DC_residual[chroma_idx].runBefore[i] = zerosLeft;
	}

	return kPARSING_ERROR_NO_ERROR;
}

int CResidual::get_chroma_AC_coeffs(int chroma_idx, int block_idc_x, int block_idc_y)
{
	int err = 0;
	int max_coeff_num = 15,i = 0;
	int numCoeff_vlcIdx = 0, prefixLength = 0, suffixLength = 0, level_prefix = 0, level_suffix = 0;

	int numberCurrent = m_macroblock_belongs->Get_number_current_chroma(chroma_idx, block_idc_x, block_idc_y);
	if (numberCurrent < 2)
	{
		numCoeff_vlcIdx = 0;
	}
	else if (numberCurrent < 4)
	{
		numCoeff_vlcIdx = 1;
	}
	else if (numCoeff_vlcIdx < 8)
	{
		numCoeff_vlcIdx = 2;
	}
	else
	{
		numCoeff_vlcIdx = 3;
	}

	// NumCoeff & TrailingOnes...
	UINT8 numCoeff = 0, trailingOnes = 0;
	int token = 0;
	err = get_numCoeff_and_trailingOnes(numCoeff, trailingOnes, token, numCoeff_vlcIdx);
	if (err < 0)
	{
		return err;
	}
	else
	{
		chroma_AC_residual[chroma_idx][block_idc_y][block_idc_x].coeffToken = token;
		chroma_AC_residual[chroma_idx][block_idc_y][block_idc_x].numCoeff = numCoeff;
		chroma_AC_residual[chroma_idx][block_idc_y][block_idc_x].trailingOnes = trailingOnes;
	}

	if (numCoeff) //包含非0系数
	{
		if (trailingOnes) //拖尾系数
		{
			//读取拖尾系数符号
			int signValue = Get_uint_code_num(m_pSODB, m_bypeOffset, m_bitOffset, trailingOnes);
			int trailingCnt = trailingOnes;
			for (int coeffIdx = 0; coeffIdx < trailingOnes; coeffIdx++)
			{
				trailingCnt--;
				if ((signValue >> trailingCnt) & 1)
				{
					chroma_AC_residual[chroma_idx][block_idc_y][block_idc_x].trailingSign[coeffIdx] = -1;
				}
				else
				{
					chroma_AC_residual[chroma_idx][block_idc_y][block_idc_x].trailingSign[coeffIdx] = 1;
				}
			}
		}

		//读取解析level值
		int level = 0;
		if (numCoeff > 10 && trailingOnes < 3)
		{
			//根据上下文初始化suffixLength
			suffixLength = 1;
		}
		for (int k = 0; k <= numCoeff - 1 - trailingOnes; k++)
		{
			err = get_coeff_level(level, k, trailingOnes, suffixLength);
			if (err < 0)
			{
				return err;
			}

			if (suffixLength == 0)
			{
				suffixLength = 1;
			}
						
			if ((abs(level) > (3 << (suffixLength - 1))) && (suffixLength < 6))
			{
				suffixLength++;
			}			

			chroma_AC_residual[chroma_idx][block_idc_y][block_idc_x].levels[k] = level;
		}

		// 读取解析run
		UINT8 zerosLeft = 0, totalZeros = 0, run = 0;
		if (numCoeff < max_coeff_num)
		{
			err = get_total_zeros(totalZeros, numCoeff - 1);
			if (err < 0)
			{
				return err;
			}
		}
		else
		{
			totalZeros = 0;
		}
		chroma_AC_residual[chroma_idx][block_idc_y][block_idc_x].totalZeros = totalZeros;

		//读取解析run_before
		int runBefore_vlcIdx = 0;
		i = numCoeff - 1;
		zerosLeft = totalZeros;
		if (zerosLeft > 0 && i > 0)
		{
			do
			{
				runBefore_vlcIdx = (zerosLeft - 1 < 6 ? zerosLeft - 1 : 6);
				err = get_run_before(run, runBefore_vlcIdx);
				if (err < 0)
				{
					return err;
				}
				chroma_AC_residual[chroma_idx][block_idc_y][block_idc_x].runBefore[i] = run;
				zerosLeft -= run;
				i--;
			} while (zerosLeft != 0 && i != 0);
		}
		else
		{
			run = 0;
		}
		chroma_AC_residual[chroma_idx][block_idc_y][block_idc_x].runBefore[i] = zerosLeft;
	}
	return kPARSING_ERROR_NO_ERROR;
}

int CResidual::get_numCoeff_and_trailingOnes(UINT8 &totalCoeff, UINT8 &trailingOnes, int &token, int numCoeff_vlcIdx)
{
	int err = 0;
	int *lengthTable = NULL, *codeTable = NULL;

	if (numCoeff_vlcIdx < 3)
	{
		lengthTable = &coeffTokenTable_Length[numCoeff_vlcIdx][0][0];
		codeTable = &coeffTokenTable_Code[numCoeff_vlcIdx][0][0];
		err = search_for_value_in_2D_table(totalCoeff, trailingOnes, token, lengthTable, codeTable, 17, 4);
		if (err < 0)
		{
			return err;
		}
	}
	else
	{
		totalCoeff = Get_uint_code_num(m_pSODB, m_bypeOffset, m_bitOffset, 4);
		trailingOnes = Get_uint_code_num(m_pSODB, m_bypeOffset, m_bitOffset, 4);
		if (!totalCoeff && trailingOnes == 3)
		{
			trailingOnes = 0;
		} 
		else
		{
			totalCoeff++;
		}
	}

	return kPARSING_ERROR_NO_ERROR;
}

int CResidual::get_coeff_level(int &level, int levelIdx, UINT8 trailingOnes, int suffixLength)
{
	int prefixLength = 0, level_prefix = 0, level_suffix = 0;
	int levelSuffixSize = 0, levelCode = 0, i = 0;

	while (!Get_bit_at_position(m_pSODB, m_bypeOffset, m_bitOffset))
	{
		level_prefix++;
	}
	prefixLength = level_prefix + 1;
	if (level_prefix == 14 && suffixLength == 0)
	{
		levelSuffixSize = 4;
	}
	else if (level_prefix == 15)
	{
		levelSuffixSize = level_prefix - 3;
	}
	else
	{
		levelSuffixSize = suffixLength;
	}
	if (levelSuffixSize > 0)
	{
		level_suffix = Get_uint_code_num(m_pSODB, m_bypeOffset, m_bitOffset, levelSuffixSize);
	}
	else
	{
		level_suffix = 0;
	}
	levelCode = (min(15, level_prefix) << suffixLength) + level_suffix;
	if (level_prefix >= 15 && suffixLength == 0)
	{
		levelCode += 15;
	}
	if (level_prefix >= 16)
	{
		levelCode += (1 << (level_prefix - 3)) - 4096;
	}
	if (levelIdx == 0 && trailingOnes < 3)
	{
		levelCode += 2;
	}

	if (levelCode % 2 == 0)
	{
		level = (levelCode + 2) >> 1;
	}
	else
	{
		level = (-levelCode - 1) >> 1;
	}

	return kPARSING_ERROR_NO_ERROR;
}

int CResidual::get_total_zeros(UINT8 &totalZeros, int totalZeros_vlcIdx)
{
	int err = 0, idx2 = 0;
	UINT8 idx1 = 0;
	int *lengthTable = &totalZerosTable_Length[totalZeros_vlcIdx][0];
	int *codeTable = &totalZerosTable_Code[totalZeros_vlcIdx][0];
	err = search_for_value_in_2D_table(totalZeros, idx1, idx2, lengthTable, codeTable, 16, 1);
	if (err < 0)
	{
		return err;
	}

	return kPARSING_ERROR_NO_ERROR;
}

int CResidual::get_run_before(UINT8 &runBefore, int runBefore_vlcIdx)
{	
	UINT8 idx1 = 0;
	int idx2 = 0, err = 0;
	int *lengthTable = &runBeforeTable_Length[runBefore_vlcIdx][0];
	int *codeTable = &runBeforeTable_Code[runBefore_vlcIdx][0];
	err = search_for_value_in_2D_table(runBefore, idx1, idx2, lengthTable, codeTable, 16, 1);
	if (err < 0)
	{
		return err;
	}

	return kPARSING_ERROR_NO_ERROR;
}

int CResidual::get_numCoeff_and_trailingOnes_chromaDC(UINT8 &totalCoeff, UINT8 &trailingOnes, int &token)
{
	int err = 0;
	int *lengthTable = &coeffTokenTableChromaDC_Length[0][0], *codeTable = &coeffTokenTableChromaDC_Code[0][0];

	err = search_for_value_in_2D_table(totalCoeff, trailingOnes, token, lengthTable, codeTable, 5, 4);
	if (err < 0)
	{
		return err;
	}

	return kPARSING_ERROR_NO_ERROR;
}

int CResidual::get_total_zeros_chromaDC(UINT8 &totalZeros, int totalZeros_vlcIdx)
{
	int err = 0, idx2 = 0;
	UINT8 idx1 = 0;
	int *lengthTable = &totalZerosTableChromaDC_Length[totalZeros_vlcIdx][0];
	int *codeTable = &totalZerosTableChromaDC_Code[totalZeros_vlcIdx][0];
	err = search_for_value_in_2D_table(totalZeros, idx1, idx2, lengthTable, codeTable, 16, 1);
	if (err < 0)
	{
		return err;
	}

	return kPARSING_ERROR_NO_ERROR;
}

int CResidual::search_for_value_in_2D_table(UINT8 &value1, UINT8 &value2, int &code, int *lengthTable, int *codeTable, int tableWidth, int tableHeight)
{
	int err = 0;
	int codeLen = 0;
	for (int yIdx = 0; yIdx < tableHeight; yIdx++)
	{
		for (int xIdx = 0; xIdx < tableWidth; xIdx++)
		{
			codeLen = lengthTable[xIdx];
			if (codeLen == 0)
			{
				continue;
			}
			code = codeTable[xIdx];
			if (Peek_uint_code_num(m_pSODB, m_bypeOffset, m_bitOffset, codeLen) == code)
			{
				value1 = xIdx;
				value2 = yIdx;
				m_bitOffset += codeLen;
				m_bypeOffset += (m_bitOffset / 8);
				m_bitOffset %= 8;
				err = 0;
				goto found;
			}
		}
		lengthTable += tableWidth;
		codeTable += tableWidth;
	}
	err = kPARSING_CAVLC_CODE_NOT_FOUND;

found:
	return err;
}
