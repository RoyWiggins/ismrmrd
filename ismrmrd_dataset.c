/* Language and Cross platform section for defining types */
#ifdef __cplusplus
#include <cstring>
#include <cstdlib>
#include <cstdio>
#else
/* C99 compiler */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#endif /* __cplusplus */

#include <hdf5.h>
#include "ismrmrd_dataset.h"

#ifdef __cplusplus
namespace ISMRMRD {
extern "C" {
#endif
    
/******************************/
/* Private (Static) Functions */
/******************************/
static bool link_exists(const ISMRMRD_Dataset *dset, const char *link_path) {

    htri_t val;
    val = H5Lexists(dset->fileid, link_path, H5P_DEFAULT);

    if (val < 0 ) {
        return false;
    }
    else if (val) {
        return true;
    }
    else {
        return false;
    }
}

static int create_link(const ISMRMRD_Dataset *dset, const char *link_path) {
    if (link_exists(dset, link_path)) {
        return ISMRMRD_NOERROR;
    }
    else {
        hid_t lcpl_id;
        lcpl_id = H5Pcreate(H5P_LINK_CREATE);
        H5Pset_create_intermediate_group(lcpl_id, 1);
        hid_t gid = H5Gcreate2(dset->fileid, link_path, lcpl_id, H5P_DEFAULT, H5P_DEFAULT);
        H5Gclose(gid);
        // TODO does this thing return error
        return ISMRMRD_NOERROR;
    }
}

static char * make_path(const ISMRMRD_Dataset *dset, const char * var) {
    size_t len = strlen(dset->groupname) + strlen(var) + 2;

    char *path = (char *) malloc(len);
    if (path == NULL) {
        ISMRMRD_THROW(ISMRMRD_MEMORYERROR, "Failed to malloc path");
        return NULL;
    }

    memset(path, 0, len);
    strcat(path, dset->groupname);
    strcat(path, "/");
    strcat(path, var);
    return path;
}

static int delete_var(const ISMRMRD_Dataset *dset, const char *var) {
    herr_t status;
    char *path = make_path(dset, var);
    if (link_exists(dset, path)) {
        status = H5Ldelete(dset->fileid, path, H5P_DEFAULT);
        // TODO handle errors
    }
    free(path);
    return ISMRMRD_NOERROR;
}

/********************/
/* Public functions */
/********************/
int ismrmrd_init_dataset(ISMRMRD_Dataset *dset, const char *filename, const char *groupname) {

    dset->filename = (char *) malloc(strlen(filename) + 1);
    if (dset->filename == NULL) {
        ISMRMRD_THROW(ISMRMRD_MEMORYERROR, "Failed to malloc dataset groupname");
        return ISMRMRD_MEMORYERROR;
    }
    strcpy(dset->filename, filename);

    dset->groupname = (char *) malloc(strlen(groupname) + 1);
    if (dset->groupname == NULL) {
        ISMRMRD_THROW(ISMRMRD_MEMORYERROR, "Failed to malloc dataset groupname");
        return ISMRMRD_MEMORYERROR;
    }
    strcpy(dset->groupname, groupname);

    dset->fileid = 0;

    return ISMRMRD_NOERROR;
}

int ismrmrd_open_dataset(ISMRMRD_Dataset *dset, const bool create_if_needed) {
    // TODO add a mode for clobbering the dataset if it exists.

    hid_t       fileid, filetype, memtype, space, daset;
    herr_t      status;

    /* Turn of HDF5 Errors */
    /* TODO, this is bad.  Maybe have it compile time dependent */
    /* or add to our error log */
    H5Eset_auto1(NULL, NULL);
    
    /* Check if the file exists and is an HDF5 File */
    status = H5Fis_hdf5(dset->filename);
    
    if (status > 0) {
        /* Positive value for exists and is HDF5 */
        /* Open it in readwrite mode */
        fileid = H5Fopen(dset->filename, H5F_ACC_RDWR, H5P_DEFAULT);
        if (fileid > 0) {
            dset->fileid = fileid;
        }
        else {
           /* Error opening the existing file */
           // TODO raise error
           return ISMRMRD_FILEERROR;
        }
    }
    else if (status == 0) {
       /* Zero value for exists and is NOT HDF5 */
       //TODO raise error
       return ISMRMRD_FILEERROR;
    }
    else {
        /* Negative value for does NOT exist or other error */
        if (create_if_needed == false) {
            ISMRMRD_THROW(ISMRMRD_FILEERROR, "Failed to open file.");
            return ISMRMRD_FILEERROR;
        }
        else {
            /* Create a new file using the default properties. */
            /* this will be readwrite */
            fileid = H5Fcreate(dset->filename, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
            if (fileid > 0) {
                dset->fileid = fileid;
            }
            else {
                /* Error creating file */
                // TODO raise error
                return ISMRMRD_FILEERROR;
            }
        }
    }

    /* Open the existing dataset */
    /* insure that /groupname exists */
    int val = create_link(dset, dset->groupname);

    return val;
};

int ismrmrd_close_dataset(ISMRMRD_Dataset *dset) {

    herr_t status;

    /* Check for a valid fileid before trying to close the file */
    if (dset->fileid > 0) {
        status = H5Fclose (dset->fileid);
        dset->fileid = 0;
    }
    return ISMRMRD_NOERROR;
};


int ismrmrd_write_header(const ISMRMRD_Dataset *dset, const char *xmlstring) {

    hid_t dataset, dataspace, datatype, props;
    hsize_t dims[] = {1};
    herr_t status;

    /* The path to the xml header */
    char *path = make_path(dset, "xml");

    /* Delete the old header if it exists */
    status = delete_var(dset, "xml");

    /* Create a new dataset for the xmlstring */
    /* i.e. create the memory type, data space, and data set */
    dataspace = H5Screate_simple(1, dims, NULL);
    datatype = H5Tcopy(H5T_C_S1);
    status = H5Tset_size(datatype, H5T_VARIABLE);
    props = H5Pcreate (H5P_DATASET_CREATE);
    dataset = H5Dcreate(dset->fileid, path, datatype, dataspace, H5P_DEFAULT, props,  H5P_DEFAULT);

    /* Write it out */
    /* We have to wrap the xmlstring in an array */
    void *buff[1];
    buff[0] = (void *) xmlstring;  /* safe to get rid of const the type */
    status = H5Dwrite(dataset, datatype, H5S_ALL, H5S_ALL, H5P_DEFAULT, buff);

    /* Clean up */
    status = H5Pclose(props);
    status = H5Tclose(datatype);
    status = H5Sclose(dataspace);
    status = H5Dclose(dataset);
    
    return ISMRMRD_NOERROR;
};

char * ismrmrd_read_header(const ISMRMRD_Dataset *dset) {

    hid_t dataset, dataspace, datatype, props;
    hsize_t dims[] = {1};
    herr_t status;
    char * xmlstring;
    
    /* The path to the xml header */
    char *path = make_path(dset, "xml");

    if (link_exists(dset, path)) {
        dataset = H5Dopen(dset->fileid, path, H5P_DEFAULT);
        /* Get the datatype */
        datatype = H5Dget_type(dataset);
        /* Read it into a 1D buffer*/
        void *buff[1];
        status = H5Dread(dataset, datatype, H5S_ALL, H5S_ALL, H5P_DEFAULT, buff);

        /* Unpack */
        xmlstring = (char *) malloc(strlen(buff[0])+1);
        if (xmlstring == NULL) {
            ISMRMRD_THROW(ISMRMRD_MEMORYERROR, "Failed to malloc xmlstring");
        } else {
            memcpy(xmlstring, buff[0], strlen(buff[0])+1);
            
            /* Clean up */
            status = H5Pclose(props);
            status = H5Tclose(datatype);
            status = H5Sclose(dataspace);
            status = H5Dclose(dataset);
        }
        free(path);
        
        return xmlstring;
    }
    else {
        // No XML String found
        // TODO handle errors
        free(path);
        return NULL;
    }

};

unsigned long ismrmrd_get_number_of_acquisitions(const ISMRMRD_Dataset *dset) {

    hid_t dataset, dataspace;
    hsize_t dims[2], maxdims[2];
    herr_t h5status;
    unsigned long numacq;
    
    /* The path to the acqusition data */    
    char *path = make_path(dset, "data");

    if (link_exists(dset, path)) {
        dataset = H5Dopen(dset->fileid, path, H5P_DEFAULT);
        dataspace = H5Dget_space(dataset);
        h5status = H5Sget_simple_extent_dims(dataspace, dims, maxdims);
        numacq = dims[0];
        H5Sclose(dataspace);
        H5Dclose(dataset);
    }
    else {
        /* none */
        numacq = 0;
    }

    free(path);

    return numacq;
};


int ismrmrd_append_acquisition(const ISMRMRD_Dataset *dset, const ISMRMRD_Acquisition *a) {

    hid_t dataset, dataspace, datatype, props;
    hsize_t dims[2];
    herr_t h5status;
    int status;
    
    /* The path to the acqusition data */    
    char *path = make_path(dset, "data");

    /* Check the path, open or create if needed */
    if (link_exists(dset, path)) {
        /* open */
    }
    else {
        /* create */
    }

    free(path);
    
    return ISMRMRD_NOERROR;
};

/*****************************/
/* TODO Implement these ones */  
/*****************************/
int ismrmrd_read_acquisition(const ISMRMRD_Dataset *dset, unsigned long index, ISMRMRD_Acquisition *a) {
    return ISMRMRD_NOERROR;
};

int ismrmrd_append_image(const ISMRMRD_Dataset *dset, const char *varname,
                         const int block_mode, const ISMRMRD_Image *im) {
    return ISMRMRD_NOERROR;
};

int ismrmrd_read_image(const ISMRMRD_Dataset *dset, const char *varname,
                       const unsigned long index, ISMRMRD_Image *im) {
    return ISMRMRD_NOERROR;
};

int ismrmrd_get_number_of_images(const ISMRMRD_Dataset *dset, const char *varname) {
    return ISMRMRD_NOERROR;
};

int ismrmrd_append_array(const ISMRMRD_Dataset *dset, const char *varname,
                         const int block_mode, const ISMRMRD_NDArray *arr) {
    return ISMRMRD_NOERROR;
};

int ismrmrd_read_array(const ISMRMRD_Dataset *dataset, const char *varname,
                       const unsigned long index, ISMRMRD_NDArray *arr) {
    return ISMRMRD_NOERROR;
};

int ismrmrd_get_number_of_arrays(const ISMRMRD_Dataset *dset, const char *varname) {
    return ISMRMRD_NOERROR;
};

    
#ifdef __cplusplus
} /* extern "C" */
} /* ISMRMRD namespace */
#endif



#ifdef YOMAMA

t = this->type_container_.get_type<float>();
t = this->type_container_.get_type<double>();
t = this->type_container_.get_type<std::complex<float> >();
t = this->type_container_.get_type<std::complex<double> >();
t = this->type_container_.get_type<EncodingCounters>();
t = this->type_container_.get_type<AcquisitionHeader>();
t = this->type_container_.get_type<AcquisitionHeader_with_data>();
t = this->type_container_.get_type<ImageHeader>();
t = this->type_container_.get_type<ImageHeader_with_data<float> >();
t = this->type_container_.get_type<ImageHeader_with_data<double> >();
t = this->type_container_.get_type<ImageHeader_with_data<unsigned short> >();
t = this->type_container_.get_type<ImageHeader_with_data<ccomplex_t> >();
t = this->type_container_.get_type<ImageHeader_with_data<cdouble_complex_t> >();
t = this->type_container_.get_type<ImageHeader_with_data<std::complex<float> > >();
t = this->type_container_.get_type<ImageHeader_with_data<std::complex<double> > >();
t = this->type_container_.get_type<std::string>();

#endif
