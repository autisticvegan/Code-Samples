
//////////////////////////////////////////////////////////////////////////////
/// _SaveSplitBasedOnIntensity
/// @brief split each scratch into high, medium, and low intensity scratches
///
/// @params[in] featureData - where we are getting the scratch
/// @params[in] splitfactor - currently unused, in the future could be 
/// the number of split deviations from the center
/// @return the vector of new scratches
//////////////////////////////////////////////////////////////////////////////
std::vector<CScratch>
CProcessorScratches::_SaveSplitBasedOnIntensity(CFeatureData& featureData, int splitfactor)
{
    //length will be measured in microns, not millimeters
    UNREFERENCED_PARAMETER(splitfactor);

    std::vector<CScratch> retvec;

    if (featureData.IsEmpty())
        return retvec;

    CScratch result;
    WoRx::PointXY minPointX;
    WoRx::PointXY maxPointX;
    WoRx::PointXY minPointY;
    WoRx::PointXY maxPointY;

    const CBlobStatistics& blobStats = featureData.p_blobStats;
    const std::vector<CPixel>& pixels = blobStats.p_blob.aPixels;
    //need pixelcount for the dividepoint
    size_t pixelCount = pixels.size();

    //need to find the points for the length. 
    // find min point x
    WoRx::PointXY pt(blobStats.calcMinX());
    if (isnan(minPointX.p_X))
        minPointX = pt;
    else
        minPointX = (pt.p_X < minPointX.p_X) ? pt : minPointX;
    // find max point x
    pt = blobStats.calcMaxX();
    if (isnan(maxPointX.p_X))
        maxPointX = pt;
    else
        maxPointX = (pt.p_X > maxPointX.p_X) ? pt : maxPointX;
    // find min point y
    pt = blobStats.calcMinY();
    if (isnan(minPointY.p_Y))
        minPointY = pt;
    else
        minPointY = (pt.p_Y < minPointY.p_Y) ? pt : minPointY;
    // find max point y
    pt = blobStats.calcMaxY();
    if (isnan(maxPointY.p_Y))
        maxPointY = pt;
    else
        maxPointY = (pt.p_Y > maxPointY.p_Y) ? pt : maxPointY;

    //are we splitting length wise or width wise (is the distance between the end points greater along x or y?)
    bool splitAlongx = false;
    int xdist = static_cast<int>(maxPointX.p_X - minPointX.p_X);
    int ydist = static_cast<int>(maxPointY.p_Y - minPointY.p_Y);
    if (xdist > ydist)
        splitAlongx = true;


    //need these values for each scratch so we dont have to recalculate each time
    result.p_Area = featureData.getBlobStats().p_area;
    result.p_Length = sqrt(pow(maxPointX.p_X - minPointX.p_X, 2) + pow(maxPointY.p_Y - minPointY.p_Y, 2));

    // Include small scratches in clustered area
    //for now, dont worry about this part with each sub scratch
    if (result.p_Length >= _surfaceParams.p_ClusterMaxScratchLength)
    {
        featureData.p_blobStats.setClassified(true);
    }

    //we will split this
    WoRx::PointXYs ptsSorted = featureData.p_blobStats.p_pts;

    //_measuredData.
    if (splitAlongx)
    {
        std::stable_sort(ptsSorted.begin(), ptsSorted.end(), xsort());
    }
    else
    {
        std::stable_sort(ptsSorted.begin(), ptsSorted.end(), ysort());
    }

    int lowerbound = 40;
    int mediumbound = 50;
    int upperbound = 60;

    //hardcode 45 and 55 for now...(maybe put this in params in the future)
    //old way: just high low so commented out, new way: high medium low
  //  std::pair<DWORD, DWORD> d = featureData.p_blob.GetIntensitySplitPointsOverRange(lowerbound, upperbound);
    std::vector<DWORD> d = featureData.p_blob.GetIntensitySplitPointsOverRange(lowerbound, mediumbound, upperbound);
    DWORD highpoint = d[2];
    DWORD lowpoint = d[0];
    DWORD medpoint = d[1];
    

    CURRINTENS low_med_or_high = LOW;
    CURRINTENS previous_meas = LOW;
    DWORD hazeval = featureData.p_blob.dwHazeValue;

    auto ptiter = ptsSorted.begin();


    std::vector<int> splitpointindices;
    int currentIndex = 0;
    int prevIndex = 0;
    //check the first one, everytime we hit a change, push back the split point
    CTrackPoint tp = _measuredData.GetPixelTrackTransform().GetTrackPoint(*ptiter);
    DWORD val = _measuredData.GetRawValueA(tp.Track(), tp.Phi()) - hazeval;

    if (val < lowpoint)
        low_med_or_high = LOW;
    else if (val > lowpoint && val < highpoint)
        low_med_or_high = MID;
    else
        low_med_or_high = HIGH;

    previous_meas = low_med_or_high;
    //note, if our split is too short, dont do the split.
    //for now, if the split is less than 20% of the total length, dont split
    double lengthsplitlimit = result.p_Length * .2;
    //future-lengthsplitlimit will be a parameter
    ptiter++;
    currentIndex++;

    //if we are splitting along x, measure distance along x


    for (; ptiter != ptsSorted.end(); ++ptiter)
    {

        tp = _measuredData.GetPixelTrackTransform().GetTrackPoint(*ptiter);
        val = _measuredData.GetRawValueA(tp.Track(), tp.Phi()) - hazeval;

        if (val > highpoint)
        {
            if (previous_meas != HIGH)
            {
                int dist = currentIndex - prevIndex;
                double ratio = (double)dist / ptsSorted.size();
                double currlength = ratio * result.p_Length;
                if (currlength > lengthsplitlimit)
                {
                    splitpointindices.push_back(currentIndex);
                    prevIndex = currentIndex;
                }
            }
            
            low_med_or_high = HIGH;
        }
        else if (val <= highpoint && val >= lowpoint)
        {
            if (previous_meas != MID)
            {
                int dist = currentIndex - prevIndex;
                double ratio = (double)dist / ptsSorted.size();
                double currlength = ratio * result.p_Length;
                if (currlength > lengthsplitlimit)
                {
                    splitpointindices.push_back(currentIndex);
                    prevIndex = currentIndex;
                }
            }

            low_med_or_high = MID;
        }
        else
        {
            if (previous_meas != LOW)
            {
                int dist = currentIndex - prevIndex;
                double ratio = (double)dist / ptsSorted.size();
                double currlength = ratio * result.p_Length;
                if (currlength > lengthsplitlimit)
                {
                    splitpointindices.push_back(currentIndex);
                    prevIndex = currentIndex;
                }

            }

            low_med_or_high = LOW;
        }


        previous_meas = low_med_or_high;   
        currentIndex++;

    }
    //if there are no points, just do it the old singular way and return
    if (splitpointindices.empty())
    {
        retvec.push_back(_Save(featureData));
        return retvec; //empty, all one scratch
    }


    //now we have the splitpoints
    //split the pixels along these points, and adjust the length, area, etc accordingly for each
    //new split scratch

    //begin...s1, s1..s2, s2...s3 etc...sn...end
    //end is the end of the pts

    for (size_t i = 0; i <= splitpointindices.size(); ++i)
    {
        CScratch newestScratch;

        //split the points
        //
        auto first = ptsSorted.begin(); 
        auto last = ptsSorted.begin(); 

        if (i == 0)
        {
            last += splitpointindices[0];
        }
        else if (i == splitpointindices.size())
        {
            first += splitpointindices[i-1];
            last = ptsSorted.end();
        }
        else
        {
            first += splitpointindices[i - 1];
            last += splitpointindices[i];
        }


        WoRx::PointXYs ptsSplit(first, last);

        WoRx::PointXY center = Centroid(ptsSplit);
        CenterSort(ptsSplit, 10000.0);
        CTrackPoint& trackPoint = _measuredData.p_PixelTrackTransform.GetTrackPoint(center);
        newestScratch.SetTrackPoint(trackPoint);
        newestScratch.SetXYPosition(center);
        newestScratch.SetPixel(featureData.getBlob().GetPeakPixel()); //are all blobs have the same peak pixel
        newestScratch.addPoints(ptsSplit, _surfaceParams.p_FlipCoordX, _surfaceParams.p_FlipCoordY);

        //set the pixel count for this scratch
        size_t pixcount = std::distance(first, last);
        newestScratch.SetPixelCount(pixcount);

        //set the mask for each one (is this even necessary? TODO)
        for (auto iter = first; iter != last; ++iter)
        {
            CTrackPoint& trackPoint = _measuredData.p_PixelTrackTransform.GetTrackPoint(*iter);

            _measuredData.SetMaskScratch(_measuredData.GetIndexFast(trackPoint.Track(), trackPoint.Phi()));
        }

        std::vector<DWORD> values(pixcount);
        size_t ix = 0;
        //split the intensity
        //
        for (auto iter = first; iter != last; ++iter)
        {
            tp = _measuredData.GetPixelTrackTransform().GetTrackPoint(*iter);
            val = _measuredData.GetRawValueA(tp.Track(), tp.Phi()) - hazeval;
            values[ix++] = val;
        }

        newestScratch.p_Intensity = WoRx::Math::MeanOfPercentileRange(values, 50, 90);

        //update the relhaze, and other vals
        newestScratch.SetValueRelHaze(newestScratch.p_Intensity);
        newestScratch.SetValueRelAPD(featureData.getBlob().GetPeakValueAbs() - _measuredData.GetAPD_BiasNoise().dBias);
        newestScratch.SetValue(featureData.getBlob().GetPeakValueAbs());

        double ratio = (double)pixcount / ptsSorted.size();
        //need to recalculate the area...
        newestScratch.p_Area = result.p_Area * ratio;
        //split the length
        //
        newestScratch.p_Length = result.p_Length * ratio;
        newestScratch.p_Defect = featureData._defect;

        retvec.push_back(newestScratch);
    }

    return retvec;
}
