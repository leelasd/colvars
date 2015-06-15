/// -*- c++ -*-

#ifndef COLVARPROXY_H
#define COLVARPROXY_H

#include <fstream>
#include <list>

#include "colvarmodule.h"
#include "colvarvalue.h"

// return values for the frame() routine
#define COLVARS_NO_SUCH_FRAME -1

// forward declarations
class colvarscript;

/// \brief Interface between the collective variables module and
/// the simulation or analysis program (NAMD, VMD, LAMMPS...).
/// This is the base class: each interfaced program is supported by a derived class.
/// Only pure virtual functions ("= 0") must be reimplemented to ensure baseline functionality.

class colvarproxy {

protected:

  /// \brief Array of 0-based integers used to uniquely associate atoms
  /// within the host program
  std::vector<int>          atoms_ids;
  /// \brief Keep track of how many times each atom is used by a separate colvar object
  std::vector<size_t>       atoms_ncopies;
  /// \brief Masses of the atoms (allow redefinition during a run, as done e.g. in LAMMPS)
  std::vector<cvm::real>    atoms_masses;
  /// \brief Current three-dimensional positions of the atoms
  std::vector<cvm::rvector> atoms_positions;
  /// \brief Most recent total forces on each atom
  std::vector<cvm::rvector> atoms_total_forces;
  /// \brief Most recent forces applied by external potentials onto each atom
  std::vector<cvm::rvector> atoms_applied_forces;
  /// \brief Forces applied from colvars, to be communicated to the MD integrator
  std::vector<cvm::rvector> atoms_new_colvar_forces;

  /// Used by all init_atom() functions: create a slot for an atom not requested yet
  inline int add_atom_slot(int atom_id)
  {
    atoms_ids.push_back(atom_id);
    atoms_ncopies.push_back(1);
    atoms_masses.push_back(1.0);
    atoms_positions.push_back(cvm::rvector(0.0));
    atoms_total_forces.push_back(cvm::rvector(0.0));
    atoms_applied_forces.push_back(cvm::rvector(0.0));
    atoms_new_colvar_forces.push_back(cvm::rvector(0.0));
    return (atoms_ids.size() - 1);
  }

  /// \brief Currently opened output files: by default, these are ofstream objects.
  /// Allows redefinition to implement different output mechanisms
  std::list<std::ostream *> output_files;
  /// \brief Identifiers for output_stream objects: by default, these are the names of the files
  std::list<std::string>    output_stream_names;

public:

  /// Pointer to the main object
  colvarmodule *colvars;

  /// Default constructor
  inline colvarproxy() : script(NULL) {}

  /// Default destructor
  virtual ~colvarproxy() {}

  /// (Re)initialize required member data after construction
  virtual void setup() {}


  // **************** SIMULATION PARAMETERS ****************

  /// \brief Value of the unit for atomic coordinates with respect to
  /// angstroms (used by some variables for hard-coded default values)
  virtual cvm::real unit_angstrom() = 0;

  /// \brief Boltzmann constant
  virtual cvm::real boltzmann() = 0;

  /// \brief Temperature of the simulation (K)
  virtual cvm::real temperature() = 0;

  /// \brief Time step of the simulation (fs)
  virtual cvm::real dt() = 0;

  /// \brief Pseudo-random number with Gaussian distribution
  virtual cvm::real rand_gaussian(void) = 0;

  /// \brief Get the current frame number
  virtual int frame() { return COLVARS_NOT_IMPLEMENTED; }

  /// \brief Set the current frame number
  // return 0 on success, -1 on failure
  virtual int frame(int) { return COLVARS_NOT_IMPLEMENTED; }

  /// \brief Prefix to be used for input files (restarts, not
  /// configuration)
  std::string input_prefix_str, output_prefix_str, restart_output_prefix_str;

  inline std::string input_prefix()
  {
    return input_prefix_str;
  }

  /// \brief Prefix to be used for output restart files
  inline std::string restart_output_prefix()
  {
    return restart_output_prefix_str;
  }

  /// \brief Prefix to be used for output files (final system
  /// configuration)
  inline std::string output_prefix()
  {
    return output_prefix_str;
  }

  /// \brief Restarts will be written each time this number of steps has passed
  virtual size_t restart_frequency() = 0;


  // **************** MULTIPLE REPLICAS COMMUNICATION ****************

  // Replica exchange commands:

  /// \brief Indicate if multi-replica support is available and active
  virtual bool replica_enabled() { return false; }

  /// \brief Index of this replica
  virtual int replica_index() { return 0; }

  /// \brief Total number of replica
  virtual int replica_num() { return 1; }

  /// \brief Synchronize replica
  virtual void replica_comm_barrier() {}

  /// \brief Receive data from other replica
  virtual int replica_comm_recv(char* msg_data, int buf_len, int src_rep) {
    return COLVARS_NOT_IMPLEMENTED;
  }

  /// \brief Send data to other replica
  virtual int replica_comm_send(char* msg_data, int msg_len, int dest_rep) {
    return COLVARS_NOT_IMPLEMENTED;
  }


  // **************** SCRIPTING INTERFACE ****************

  /// Pointer to the scripting interface object
  /// (does not need to be allocated in a new interface)
  colvarscript *script;

  /// is a user force script defined?
  bool force_script_defined;

  /// Do we have a scripting interface?
  bool have_scripts;

  /// Run a user-defined colvar forces script
  virtual int run_force_callback() { return COLVARS_NOT_IMPLEMENTED; }

  virtual int run_colvar_callback(std::string const &name,
                                  std::vector<const colvarvalue *> const &cvcs,
                                  colvarvalue &value)
  { return COLVARS_NOT_IMPLEMENTED; }

  virtual int run_colvar_gradient_callback(std::string const &name,
                                           std::vector<const colvarvalue *> const &cvcs,
                                           std::vector<cvm::matrix2d<cvm::real> > &gradient)
  { return COLVARS_NOT_IMPLEMENTED; }


  // **************** INPUT/OUTPUT ****************

  /// Print a message to the main log
  virtual void log(std::string const &message) = 0;

  /// Print a message to the main log and let the rest of the program handle the error
  virtual void error(std::string const &message) = 0;

  /// Print a message to the main log and exit with error code
  virtual void fatal_error(std::string const &message) = 0;

  /// Print a message to the main log and exit normally
  virtual void exit(std::string const &message) = 0;

  // TODO the following definitions may be moved to a .cpp file

  /// \brief Returns a reference to the given output channel;
  /// if this is not open already, then open it
  virtual std::ostream * output_stream(std::string const &output_name)
  {
    std::list<std::ostream *>::iterator osi  = output_files.begin();
    std::list<std::string>::iterator    osni = output_stream_names.begin();
    for ( ; osi != output_files.end(); osi++, osni++) {
      if (*osni == output_name) {
        return *osi;
      }
    }
    output_stream_names.push_back(output_name);
    std::ofstream * os = new std::ofstream(output_name.c_str());
    if (!os->is_open()) {
      cvm::error("Error: cannot write to file \""+output_name+"\".\n",
                 FILE_ERROR);
    }
    output_files.push_back(os);
    return os;
  }

  /// \brief Closes the given output channel
  virtual int close_output_stream(std::string const &output_name)
  {
    std::list<std::ostream *>::iterator osi  = output_files.begin();
    std::list<std::string>::iterator    osni = output_stream_names.begin();
    for ( ; osi != output_files.end(); osi++, osni++) {
      if (*osni == output_name) {
        ((std::ofstream *) (*osi))->close();
        output_files.erase(osi);
        output_stream_names.erase(osni);
        return COLVARS_OK;
      }
    }
    cvm::error("Error: trying to close an output file or stream that wasn't open.\n",
               BUG_ERROR);
    return COLVARS_ERROR;
  }

  /// \brief Rename the given file, before overwriting it
  virtual int backup_file(char const *filename)
  {
    return COLVARS_NOT_IMPLEMENTED;
  }



  // **************** ACCESS SYSTEM DATA ****************

  /// Pass restraint energy value for current timestep to MD engine
  virtual void add_energy(cvm::real energy) = 0;

  /// Tell the proxy whether system forces are needed (may not always be available)
  virtual void request_system_force(bool yesno)
  {
    if (yesno == true)
      cvm::error("Error: system forces are currently not implemented.\n",
                 COLVARS_NOT_IMPLEMENTED);
  }

  /// \brief Get the PBC-aware distance vector between two positions
  virtual cvm::rvector position_distance(cvm::atom_pos const &pos1,
                                         cvm::atom_pos const &pos2) = 0;

  /// \brief Get the PBC-aware square distance between two positions;
  /// may need to be reimplemented independently from position_distance() for optimization purposes
  virtual cvm::real position_dist2(cvm::atom_pos const &pos1,
                                   cvm::atom_pos const &pos2)
  {
    return (position_distance(pos1, pos2)).norm2();
  }

  /// \brief Get the closest periodic image to a reference position
  /// \param pos The position to look for the closest periodic image
  /// \param ref_pos The reference position
  virtual void select_closest_image(cvm::atom_pos &pos,
                                    cvm::atom_pos const &ref_pos) = 0;

  /// \brief Perform select_closest_image() on a set of atomic positions
  ///
  /// After that, distance vectors can then be calculated directly,
  /// without using position_distance()
  void select_closest_images(std::vector<cvm::atom_pos> &pos,
                             cvm::atom_pos const &ref_pos)
  {
    for (std::vector<cvm::atom_pos>::iterator pi = pos.begin();
         pi != pos.end(); ++pi) {
      select_closest_image(*pi, ref_pos);
    }
  }


  // **************** ACCESS ATOMIC DATA ****************

  /// Prepare this atom for collective variables calculation, selecting it by numeric index (1-based)
  virtual int init_atom(int atom_number) = 0;

  /// Select this atom for collective variables calculation, using name and residue number.
  /// Not all programs support this: leave this function as is in those cases.
  virtual int init_atom(cvm::residue_id const &residue,
                        std::string const     &atom_name,
                        std::string const     &segment_id)
  {
    cvm::error("Error: initializing an atom by name and residue number is currently not supported.\n",
               COLVARS_NOT_IMPLEMENTED);
    return -1;
  }

  /// \brief Used by the atom class destructor: rather than deleting the array slot
  /// (costly) set the corresponding atoms_copies to zero
  virtual void clear_atom(int index)
  {
    if ( (index < 0) || (index >= atoms_ids.size()) ) {
      cvm::error("Error: trying to disable an atom that was not previously requested.\n",
                 INPUT_ERROR);
    }
    if (atoms_ncopies[index] > 0) {
      atoms_ncopies[index] -= 1;
    }
  }

  /// Get the numeric ID of the given atom (for the program)
  inline cvm::real get_atom_id(int index) const
  {
    return atoms_ids[index];
  }

  /// Get the mass of the given atom
  inline cvm::real get_atom_mass(int index) const
  {
    return atoms_masses[index];
  }

  /// Read the current position of the given atom
  inline cvm::rvector get_atom_position(int index) const
  {
    return atoms_positions[index];
  }

  /// Read the current total system force of the given atom
  inline cvm::rvector get_atom_system_force(int index) const
  {
    return atoms_total_forces[index] - atoms_applied_forces[index];
  }

  /// Request that this force is applied to the given atom
  inline void apply_atom_force(int index, cvm::rvector const &new_force)
  {
    atoms_new_colvar_forces[index] += new_force;
  }

  /// Read the current velocity of the given atom
  virtual cvm::rvector get_atom_velocity(int index)
  {
    cvm::error("Error: reading the current velocity of an atom is not yet implemented.\n",
               COLVARS_NOT_IMPLEMENTED);
    return cvm::rvector(0.0);
  }

  /// \brief Read atom identifiers from a file \param filename name of
  /// the file (usually a PDB) \param atoms array to which atoms read
  /// from "filename" will be appended \param pdb_field (optiona) if
  /// "filename" is a PDB file, use this field to determine which are
  /// the atoms to be set
  virtual int load_atoms(char const *filename,
                         std::vector<cvm::atom> &atoms,
                         std::string const &pdb_field,
                         double const pdb_field_value = 0.0) = 0;

  /// \brief Load the coordinates for a group of atoms from a file
  /// (usually a PDB); if "pos" is already allocated, the number of its
  /// elements must match the number of atoms in "filename"
  virtual int load_coords(char const *filename,
                          std::vector<cvm::atom_pos> &pos,
                          const std::vector<int> &indices,
                          std::string const &pdb_field,
                          double const pdb_field_value = 0.0) = 0;

};


#endif
