/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsVoidArray.h"
#include "nsCellMap.h"
#include "nsTableFrame.h"
#include "nsTableCellFrame.h"

nsCellMap::nsCellMap(int aRowCount, int aColCount)
  : mRowCount(0),
    mNumCollapsedRows(0),
    mNumCollapsedCols(0),
    mNextAvailRowIndex(0)
{
  mIsCollapsedRows = nsnull;
  mIsCollapsedCols = nsnull;

  Grow(aRowCount, aColCount);
}

nsCellMap::~nsCellMap()
{
  PRInt32 mapRowCount = mRows.Count();
  PRInt32 colCount    = mNumCellsInCol.Count();
  PRInt32 colX;
  for (PRInt32 rowX = 0; rowX < mapRowCount; rowX++) {
    nsVoidArray* row = (nsVoidArray *)(mRows.ElementAt(rowX));
    for (colX = 0; colX <= colCount; colX++) {
      CellData* data = (CellData *)(row->ElementAt(colX));
      if (data) {
        delete data;
      } 
    }
    delete row;
  }
  for (colX = 0; colX < colCount; colX++) {
    PRInt32* val = (PRInt32 *)mNumCellsInCol.ElementAt(colX);
    if (val) {
      delete [] val;
    }
  }

  if (nsnull != mIsCollapsedRows) {
    delete [] mIsCollapsedRows;
    mIsCollapsedRows = nsnull;
    mNumCollapsedRows = 0;
  }
  if (nsnull != mIsCollapsedCols) {
    delete [] mIsCollapsedCols;
    mIsCollapsedCols = nsnull;
    mNumCollapsedCols = 0;
  }
};

void nsCellMap::AddColsAtEnd(PRUint32 aNumCols)
{
  Grow(mRowCount, mNumCellsInCol.Count() + aNumCols);
}

PRInt32 nsCellMap::GetNextAvailRowIndex()
{
  return mNextAvailRowIndex++;
}

void nsCellMap::Grow(PRInt32 aNumMapRows, 
                     PRInt32 aNumCols)
{
  PRInt32 origNumMapRows = mRows.Count();
  PRInt32 origNumCols    = mNumCellsInCol.Count();

  // if the number of columns has increased, we need to add extra cols to mNumColsOriginating and
  // each row in mRows.
  if (origNumCols < aNumCols) {
    PRInt32 colX;
    for (PRInt32 rowX = 0; rowX < origNumMapRows; rowX++) {
      nsVoidArray* row = (nsVoidArray *)(mRows.ElementAt(rowX));
      const PRInt32 colsInRow = row->Count();
      for (colX = colsInRow; colX < aNumCols; colX++) {
        row->AppendElement(nsnull);
      }
    }
    // update the col array 
    for (colX = origNumCols; colX < aNumCols; colX++) {
      PRInt32* val = new PRInt32[2];
      val[0] = val[1] = 0;
      mNumCellsInCol.AppendElement(val);
    }
  }

  // if the number of rows has increased, add the extra rows. the new
  // rows get the new number of cols
  PRInt32 newRows = aNumMapRows - origNumMapRows; 
  for ( ; newRows > 0; newRows--) {
    nsVoidArray* row;
    row = (0 == aNumCols) ? new nsVoidArray() : new nsVoidArray(aNumCols);
    mRows.AppendElement(row);
  }
}

PRInt32 nsCellMap::AppendCell(nsTableCellFrame* aCellFrame, 
                              PRInt32           aRowIndex)
{
  NS_ASSERTION(aCellFrame, "bad cell frame");

  PRInt32 origNumMapRows = mRows.Count();
  PRInt32 origNumCols    = mNumCellsInCol.Count();

  // get the first null CellData in the desired row. It may be 1 past the end if there are none
  PRInt32 startColIndex;
  if (aRowIndex < origNumMapRows) {
    for (startColIndex = 0; startColIndex < origNumCols; startColIndex++) {
      CellData* data = GetMapCellAt(aRowIndex, startColIndex);
      if (!data) {
        break; // we found the col
      }
    }
  }
  else {
    startColIndex = 0;
  }

  mRowCount = PR_MAX(mRowCount, aRowIndex + 1);

  PRInt32 rowSpan = aCellFrame->GetRowSpan();
  PRInt32 colSpan = aCellFrame->GetColSpan();
  // Grow. we may need to add new rows/cols 
  PRInt32 spanNumRows = aRowIndex + rowSpan;
  PRInt32 spanNumCols = startColIndex + colSpan;
  if ((spanNumRows >= origNumMapRows) || (spanNumCols >= origNumCols)) {
    Grow(spanNumRows, spanNumCols);
  }

  // Setup CellData for this cell
  CellData* origData = new CellData(aCellFrame, nsnull, nsnull);
  SetMapCellAt(*origData, aRowIndex, startColIndex);

  // reset the col index of the cell frame
  PRInt32 oldColIndex;
  aCellFrame->GetColIndex(oldColIndex);
  if (startColIndex != oldColIndex) { 
    // only do this if they are different since it changes content
    aCellFrame->SetColIndex(startColIndex); 
  }

  // Create CellData objects for the rows that this cell spans. Set
  // their mOrigCell to nsnull and their mSpanData to point to data.
  for (PRInt32 rowX = aRowIndex; rowX < spanNumRows; rowX++) {
    for (PRInt32 colX = startColIndex; colX < spanNumCols; colX++) {
      if ((rowX != aRowIndex) || (colX != startColIndex)) { // skip orig cell data done above
        CellData* cellData = GetMapCellAt(rowX, colX);
        if (cellData) {
          NS_ASSERTION(!cellData->mOrigCell, "cannot overlap originating cell");
          if (rowX > aRowIndex) { // row spanning into cell
            if (cellData->mRowSpanData) {
              NS_ASSERTION(PR_FALSE, "too many overlaps");
            }
            else {
              cellData->mRowSpanData = origData;
            }
          }
          if (colX > startColIndex) { // col spanning into cell
            if (cellData->mColSpanData) {
              NS_ASSERTION(PR_FALSE, "too many overlaps");
            }
            else {
              cellData->mColSpanData = origData;
            }
          }
        }
        else { 
          cellData = new CellData(nsnull, nsnull, nsnull);
          if (rowX > aRowIndex) 
            cellData->mRowSpanData = origData;
          if (colX > startColIndex) 
            cellData->mColSpanData = origData;
          SetMapCellAt(*cellData, rowX, colX);
        }
      }
    }
  }
  return startColIndex;
}

void nsCellMap::RemoveCell(nsTableCellFrame* aCellFrame,
                           PRInt32           aRowIndex)
{
  // XXX write me. For now the cell map is recalculate from scratch when a cell is deleted
}

PRInt32 nsCellMap::GetNumCellsIn(PRInt32 aColIndex, 
                                 PRBool aOriginating) const
{
  PRInt32 colCount = mNumCellsInCol.Count();
  if ((aColIndex >= 0) && (aColIndex < colCount)) {
    PRInt32* numCellsArray = (PRInt32 *)mNumCellsInCol.ElementAt(aColIndex);
    PRInt32 numCells = (aOriginating) ? *numCellsArray : *(numCellsArray + 1);
    return numCells;
  }
  else {
    NS_ASSERTION(PR_FALSE, "nsCellMap::GetNumCellsIn - bad col index");
    return 0;
  }
}

PRInt32 nsCellMap::GetNumCellsIn(PRInt32 aColIndex) const
{
  return GetNumCellsIn(aColIndex, 1);
}
  
PRInt32 nsCellMap::GetNumCellsOriginatingIn(PRInt32 aColIndex) const
{
  return GetNumCellsIn(aColIndex, 0);
}

#ifdef NS_DEBUG
void nsCellMap::Dump() const
{
  printf("***** start CellMap Dump *****\n");
  PRInt32 mapRowCount = mRows.Count();
  PRInt32 colCount = mNumCellsInCol.Count();
  PRInt32 colIndex;
  printf("mapRowCount=%d tableRowCount=%d, colCount=%d \n", mapRowCount, mRowCount, colCount);

  for (PRInt32 rowIndex = 0; rowIndex < mapRowCount; rowIndex++) {
    nsVoidArray* row = (nsVoidArray *)mRows.ElementAt(rowIndex);
    printf("row %d : ", rowIndex);
    for (colIndex = 0; colIndex < colCount; colIndex++) {
      CellData* cd = (CellData *)row->ElementAt(colIndex);
      if (cd) {
        if (cd->mOrigCell) {
          printf("C%d,%d  ", rowIndex, colIndex);
        } else {
          nsTableCellFrame* cell = nsnull;
          int rr, cc;
          if (cd->mRowSpanData) {
            nsTableCellFrame* cell = cd->mRowSpanData->mOrigCell;
            nsTableRowFrame* row;
            cell->GetParent((nsIFrame**)&row);
            rr = row->GetRowIndex();
            cell->GetColIndex(cc);
            printf("r%d,%d  ", rr, cc);
          }
          if (cd->mColSpanData){
            cell = cd->mColSpanData->mOrigCell;
            nsTableRowFrame* row2;
            cell->GetParent((nsIFrame**)&row2);
            rr = row2->GetRowIndex();
            cell->GetColIndex(cc);
            printf("c%d,%d ", rr, cc);
          } 
        }
      } else {
        printf("----      ");
      }
    }
    printf("\n");
  }

  // output info mapping Ci,j to cell address
  PRInt32 cellCount = 0;
  for (PRInt32 rIndex = 0; rIndex < mapRowCount; rIndex++) {
    nsVoidArray* row = (nsVoidArray *)mRows.ElementAt(rIndex);
    for (colIndex = 0; colIndex < colCount; colIndex++) {
      CellData* cd = (CellData *)row->ElementAt(colIndex);
      if (cd) {
        if (cd->mOrigCell) {
          nsTableCellFrame* cellFrame = cd->mOrigCell;
          PRInt32 cellFrameColIndex;
          cellFrame->GetColIndex(cellFrameColIndex);
          printf("C%d,%d=%p(%d)  ", rIndex, colIndex, cellFrame, cellFrameColIndex);
          cellCount++;
        }
      }
    }
    printf("\n");
  }

	// output col frame info
  printf("\n col frames ->");
	for (colIndex = 0; colIndex < mColFrames.Count(); colIndex++) {
    nsTableColFrame* colFrame = (nsTableColFrame *)mColFrames.ElementAt(colIndex);
    if (0 == (colIndex % 8))
      printf("\n");
    printf ("%d=%p ", colIndex, colFrame);
	}
  // output cells originating in cols
	printf ("\ncells in/originating in cols array -> ");
	for (colIndex = 0; colIndex < colCount; colIndex++) {
    PRInt32* numCells = (PRInt32 *)mNumCellsInCol.ElementAt(colIndex);
		printf ("%d=%d,%d ", colIndex, *numCells, *(numCells + 1));
	}
  printf("\n");
  printf("\n***** end CellMap Dump *****\n");
}
#endif

// only called if the cell at aMapRowIndex, aColIndex is null
void nsCellMap::SetMapCellAt(CellData& aNewCell, 
                             PRInt32   aMapRowIndex, 
                             PRInt32   aColIndex)
{
  nsVoidArray* row = (nsVoidArray *)(mRows.ElementAt(aMapRowIndex));
  row->ReplaceElementAt(&aNewCell, aColIndex);
  // update the col array
  PRInt32* numCells = (PRInt32 *)mNumCellsInCol.ElementAt(aColIndex);
  if (aNewCell.mOrigCell) { // cell originates in this col
    (*numCells)++;
  }
  (*(numCells + 1))++;         // cell occupies this col
}

PRInt32 nsCellMap::GetEffectiveColSpan(PRInt32                 aColIndex, 
                                       const nsTableCellFrame* aCell) const
{
  NS_PRECONDITION(nsnull != aCell, "bad cell arg");

  PRInt32 initialRowX;
  aCell->GetRowIndex(initialRowX);

  PRInt32 effColSpan = 0;
  PRInt32 colCount = mNumCellsInCol.Count();
  for (PRInt32 colX = aColIndex; colX < colCount; colX++) {
    PRBool found = PR_FALSE;
    CellData* cellData = GetCellAt(initialRowX, colX);
    if (cellData) {
      if (cellData->mOrigCell) {
        if (cellData->mOrigCell == aCell) {
          found = PR_TRUE;
        }
      }
      else if (cellData->mColSpanData && 
              (cellData->mColSpanData->mOrigCell == aCell)) {
        found = PR_TRUE;
      }
    }
    if (found) {
      effColSpan++;
    }
    else {
      break;
    }
  }
  NS_ASSERTION(effColSpan > 0, "invalid col span or col index");
  return effColSpan;
}

nsTableCellFrame* nsCellMap::GetCellFrameOriginatingAt(PRInt32 aRowX, 
                                                       PRInt32 aColX) const
{
  CellData* data = GetCellAt(aRowX, aColX);
  if (data) {
    return data->mOrigCell;
  }
  return nsnull;
}

nsTableCellFrame* nsCellMap::GetCellInfoAt(PRInt32  aRowX, 
                                           PRInt32  aColX, 
                                           PRBool*  aOriginates, 
                                           PRInt32* aColSpan) const
{
  if (aOriginates)
    *aOriginates = PR_FALSE;
  CellData* data = GetCellAt(aRowX, aColX);
  nsTableCellFrame* cellFrame = nsnull;  
  if (data) {
    if (data->mOrigCell) {
      cellFrame = data->mOrigCell;
      if (aOriginates)
        *aOriginates = PR_TRUE;
      if (aColSpan) {
        PRInt32 initialColIndex;
        cellFrame->GetColIndex(initialColIndex);
        *aColSpan = GetEffectiveColSpan(initialColIndex, cellFrame);
      }
    }
    else {
      if (data->mRowSpanData) {
        cellFrame = data->mRowSpanData->mOrigCell;
      }
      else if (data->mColSpanData) {
        cellFrame = data->mColSpanData->mOrigCell;
      }
      if (aColSpan)
        *aColSpan = 0;
    }
  }
  return cellFrame;
}
  

                                 
nsTableColFrame* nsCellMap::GetColumnFrame(PRInt32 aColIndex) const
{
  return (nsTableColFrame *)(mColFrames.ElementAt(aColIndex));
}

PRInt32 nsCellMap::GetNumCollapsedRows() const
{
  return mNumCollapsedRows;
}

PRBool nsCellMap::IsRowCollapsedAt(PRInt32 aRow) const
{
  if ((aRow >= 0) && (aRow < mRowCount)) {
    if (mIsCollapsedRows) {
      return mIsCollapsedRows[aRow];
    }
  }
  return PR_FALSE;
}

void nsCellMap::SetRowCollapsedAt(PRInt32 aRow, PRBool aValue)
{
  if ((aRow >= 0) && (aRow < mRowCount)) {
    if (nsnull == mIsCollapsedRows) {
      mIsCollapsedRows = new PRPackedBool[mRowCount];
      for (PRInt32 i = 0; i < mRowCount; i++) {
        mIsCollapsedRows[i] = PR_FALSE;
      }
    }
    if (mIsCollapsedRows[aRow] != aValue) {
      if (PR_TRUE == aValue) {
        mNumCollapsedRows++;
      } else {
        mNumCollapsedRows--;
      }
      mIsCollapsedRows[aRow] = aValue; 
    }
  }
}

PRInt32 nsCellMap::GetNumCollapsedCols() const
{
  return mNumCollapsedCols;
}

PRBool nsCellMap::IsColCollapsedAt(PRInt32 aCol) const
{
  PRInt32 colCount = mNumCellsInCol.Count();
  if ((aCol >= 0) && (aCol < colCount)) {
    if (mIsCollapsedCols) {
      return mIsCollapsedCols[aCol];
    }
  }
  return PR_FALSE;
}

void nsCellMap::SetColCollapsedAt(PRInt32 aCol, PRBool aValue)
{
  PRInt32 colCount = mNumCellsInCol.Count();
  if ((aCol >= 0) && (aCol < colCount)) {
    if (nsnull == mIsCollapsedCols) {
      mIsCollapsedCols = new PRPackedBool[colCount];
      for (PRInt32 i = 0; i < colCount; i++) {
        mIsCollapsedCols[i] = PR_FALSE;
      }
    }
    if (mIsCollapsedCols[aCol] != aValue) {
      if (PR_TRUE == aValue) {
        mNumCollapsedCols++;
      } else {
        mNumCollapsedCols--;
      }
      mIsCollapsedCols[aCol] = aValue; 
    }
  }
}

PRBool nsCellMap::RowIsSpannedInto(PRInt32 aRowIndex) const
{
  NS_PRECONDITION ((0 <= aRowIndex) && (aRowIndex < mRowCount), "bad row index arg");
  PRInt32 colCount = mNumCellsInCol.Count();
	for (PRInt32 colIndex = 0; colIndex < colCount; colIndex++) {
		CellData* cd = GetCellAt(aRowIndex, colIndex);
		if (cd) { // there's really a cell at (aRowIndex, colIndex)
			if (!cd->mOrigCell) { // the cell at (aRowIndex, colIndex) is the result of a span
				if (nsnull != cd->mRowSpanData->mOrigCell)
          return PR_TRUE;
			}
		}
  }
  return PR_FALSE;
}

PRBool nsCellMap::RowHasSpanningCells(PRInt32 aRowIndex) const
{
  NS_PRECONDITION ((0 <= aRowIndex) && (aRowIndex < mRowCount), "bad row index arg");
  PRInt32 colCount = mNumCellsInCol.Count();
  if (aRowIndex != mRowCount - 1) {
    // aRowIndex is not the last row, so we check the next row after aRowIndex for spanners
    for (PRInt32 colIndex = 0; colIndex < colCount; colIndex++) {
      CellData* cd = GetCellAt(aRowIndex, colIndex);
      if (cd && (cd->mOrigCell)) { // cell originates 
        CellData* cd2 = GetCellAt(aRowIndex + 1, colIndex);
        if (cd2 && !cd2->mOrigCell && cd2->mRowSpanData) { // cd2 is spanned by a row
          if (cd->mOrigCell == cd2->mRowSpanData->mOrigCell)
            return PR_TRUE;
        }
      }
    }
  }
  return PR_FALSE;
}

PRBool nsCellMap::ColIsSpannedInto(PRInt32 aColIndex) const
{
  NS_PRECONDITION ((0 <= aColIndex) && (aColIndex < mNumCellsInCol.Count()), "bad col index arg");
	for (PRInt32 rowIndex = 0; rowIndex < mRowCount; rowIndex++) {
		CellData* cd = GetCellAt(rowIndex, aColIndex);
		if (cd) { // there's really a cell at (aRowIndex, colIndex)
			if (!cd->mOrigCell) { // the cell at (aRowIndex, colIndex) is the result of a span
				if (nsnull != cd->mColSpanData->mOrigCell)
          return PR_TRUE;
			}
		}
  }
  return PR_FALSE;
}

PRBool nsCellMap::ColHasSpanningCells(PRInt32 aColIndex) const
{
  NS_PRECONDITION (aColIndex < mNumCellsInCol.Count(), "bad col index arg");
  PRInt32 colCount = mNumCellsInCol.Count();
  if (aColIndex >= colCount - 1)
    return PR_FALSE;
  for (PRInt32 rowIndex = 0; rowIndex < mRowCount; rowIndex++) {
    CellData* cd = GetCellAt(rowIndex, aColIndex);
    if (cd && (cd->mOrigCell)) { // cell originates 
      CellData* cd2 = GetCellAt(rowIndex + 1, aColIndex);
      if (cd2 && !cd2->mOrigCell && cd2->mColSpanData) { // cd2 is spanned by a col
        if (cd->mOrigCell == cd2->mColSpanData->mOrigCell)
          return PR_TRUE;
      }
    }
  }
  return PR_FALSE;
}


