#include "get-nodes.h"
#include <unordered_set>
#include <Rcpp.h>

using namespace Rcpp;

const float FLOAT_MAX = std::numeric_limits<float>::max ();

// direct copy of version from get-ways.cpp, but loading that header causes all
// sorts of re-definition issues, so it's easier just to redefine this function
// here.
Rcpp::NumericMatrix rcpp_get_bbox2 (float xmin, float xmax, float ymin, float ymax)
{
    std::vector <std::string> colnames, rownames;
    colnames.push_back ("min");
    colnames.push_back ("max");
    rownames.push_back ("x");
    rownames.push_back ("y");
    List dimnames (2);
    dimnames (0) = rownames;
    dimnames (1) = colnames;

    NumericMatrix bbox (Dimension (2, 2));
    bbox (0, 0) = xmin;
    bbox (0, 1) = xmax;
    bbox (1, 0) = ymin;
    bbox (1, 1) = ymax;

    bbox.attr ("dimnames") = dimnames;

    return bbox;
};


// [[Rcpp::export]]
Rcpp::S4 rcpp_get_nodes (std::string st)
{
    XmlNodes xmlNodes (st);

    int coli, rowi;
    float lon, lat;
    float xmin = FLOAT_MAX, xmax = -FLOAT_MAX, 
          ymin = FLOAT_MAX, ymax = -FLOAT_MAX;
    std::string key;
    std::vector <float> lons, lats;
    std::vector <std::string> colnames, rownames, varnames;
    Rcpp::List dimnames (0);
    Rcpp::NumericMatrix nmat (Dimension (0, 0));

    colnames.push_back ("lon");
    colnames.push_back ("lat");

    std::vector <std::pair <std::string, std::string> >::iterator kv_iter;

    for (Nodes_Itr ni = xmlNodes.nodes.begin ();
            ni != xmlNodes.nodes.end (); ++ni)
    {
        for (kv_iter = (*ni).key_val.begin (); kv_iter != (*ni).key_val.end ();
                ++kv_iter)
        {
            key = (*kv_iter).first;
            if (std::find (varnames.begin (), varnames.end (), key) == varnames.end ())
                varnames.push_back (key);
        }
        
        lon = (*ni).lon;
        lat = (*ni).lat;
        lons.push_back (lon);
        lats.push_back (lat);
        if (lon < xmin)
            xmin = lon;
        else if (lon > xmax)
            xmax = lon;
        if (lat < ymin)
            ymin = lat;
        else if (lat > ymax)
            ymax = lat;

        rownames.push_back (std::to_string ((*ni).id));
    }

    // Store all key-val pairs in one massive DF
    int nrow = xmlNodes.nodes.size (), ncol = varnames.size ();
    Rcpp::CharacterVector kv_vec (nrow * ncol, Rcpp::CharacterVector::get_na());
    for (Nodes_Itr ni = xmlNodes.nodes.begin ();
            ni != xmlNodes.nodes.end (); ++ni)
    {
        for (kv_iter = (*ni).key_val.begin (); kv_iter != (*ni).key_val.end ();
                ++kv_iter)
        {
            key = (*kv_iter).first;
            auto it = std::find (varnames.begin (), varnames.end (), key);
            // key must exist in varnames!
            coli = it - varnames.begin (); 
            rowi = ni - xmlNodes.nodes.begin ();
            kv_vec (coli * nrow + rowi) = (*kv_iter).second;
        }
    }

    nmat = Rcpp::NumericMatrix (Dimension (lons.size (), 2));
    std::copy (lons.begin (), lons.end (), nmat.begin ());
    std::copy (lats.begin (), lats.end (), nmat.begin () + lons.size ());
    dimnames.push_back (rownames);
    dimnames.push_back (colnames);
    nmat.attr ("dimnames") = dimnames;
    while (dimnames.size () > 0)
        dimnames.erase (0);

    Rcpp::CharacterMatrix kv_mat (nrow, ncol, kv_vec.begin());
    Rcpp::DataFrame kv_df = kv_mat;
    kv_df.attr ("names") = varnames;
    
    Rcpp::Language points_call ("new", "SpatialPoints");
    Rcpp::Language sp_points_call ("new", "SpatialPointsDataFrame");
    Rcpp::S4 sp_points;
    sp_points = sp_points_call.eval ();
    sp_points.slot ("data") = kv_df;
    sp_points.slot ("coords") = nmat;
    sp_points.slot ("bbox") = rcpp_get_bbox2 (xmin, xmax, ymin, ymax);

    Rcpp::Language crs_call ("new", "CRS");
    Rcpp::S4 crs = crs_call.eval ();
    crs.slot ("projargs") = "+proj=longlat +ellps=WGS84 +datum=WGS84 +no_defs +towgs84=0,0,0";
    sp_points.slot ("proj4string") = crs;

    lons.resize (0);
    lats.resize (0);
    colnames.resize (0);
    rownames.resize (0);
    varnames.resize (0);

    return sp_points;
}