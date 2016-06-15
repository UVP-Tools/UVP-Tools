    UVP Tools 2.2.0.xxx
The following is the readme file of UVP Tools 2.2.0. From the readme file, you will learn what a UVP Tools project is, how source code of UVP Tools is structured, and how UVP Tools is installed.

What Is a UVP Tools Project?
UVP Tools is a tool that integrates the Xen front-end driver and uvp-monitor (virtual machine monitoring program). It is designed for use on virtual machines (VMs) equipped with a 32-bit x86-based CPU (386 or higher) or x86_64 CPU.
The Xen front-end driver improves I/O processing efficiency of VMs. The uvp-monitor helps administrators gain visibility into VM running status.
Components of the Xen front-end driver:
  - xen-platform-pci driver: Xen front-end pci driver, which provides xenbus access, establishes event channels, and grants references.
  - xen-balloon driver: Xen front-end balloon driver, which adjusts VM memory through the host.
  - xen-vbd/xen-blkfront driver: Xen front-end disk driver.
  - xen-vnif/xen-netfront driver: Xen front-end NIC driver.
  - xen-scsi/xen-scsifront driver: Xen front-end PV SCSI driver.
  - xen-hcall driver: synchronizes VM time with host clock through Xen hypercall.
  - xen-procfs driver: provides the xenbus driver and adapts to the VMs that use pvops kernel but does not provide /proc/xen/xenbus or /dev/xen/xenbus interfaces.
  - vni driver: virtio driver.

Features of uvp-monitor:
  - Collects information about resource usage of every VM and periodically writes the information into domain0 through xenstore. The resource usage information includes the total number of CPUs. CPU usage, memory capacity, memory usage, disk capacity, disk usage, and the number of packets received or sent by a NIC.
  - Collects VM hostnames and writes them into domain0 through xenstore.
  - Notifies domain0 whether the current VM supports hot-plug of memory, CPUs, and disks.
  - Facilitates live migration of Xen VMs and triggers VMs to send gARP and ndp packets to the network at completion of live migration.
  - Facilitates self-upgrade of UVP Tools on VMs. The uvp-monitor works with xenstore to notify domain0 of the UVP Tools version installed on VMs. If the UVP Tools version on a VM is different from the UVP Tools version on the host, the UVP Tools on the VM is then automatically upgraded to the version installed on the host.

Structure of UVP Tools source code:
UVP-Tools-2.2.0.xxx
|-- bin/             # Directory that stores tools required for installing and using UVP Tools, such as the tool used for acquiring Linux distribution information and the disk hot-plug tool.
|-- build_tools      # Scripts used for building the UVP Tools package.
|-- config/          # Directory that stores UDEV rules used by UVP Tools.
|-- cpfile.sh        # Tool for copying components of the UVP Tools compatible with the current Linux distribution during self-upgrade of UVP Tools.
|-- install          # Scripts for installing, uninstalling, and upgrading the UVP Tools package.
|-- Makefile         # File that define rules for building the UVP Tools package.
|-- README           # Readme file of UVP Tools.
|-- upg.sh           # Script for self-upgrading UVP Tools on a VM to the UVP Tools version installed on the host.
|-- uvp-monitor/     # Source code of uvp-monitor.
|-- uvp-xenpv/       # Source code of the Xen front-end driver.
| |-- uvp-classic_xen_driver-2.6.32to3.0.x/   # Source code of the classic Xen front-end driver, which works well with SLES 11 SP.
| |-- uvp-classic_xen_driver-3.12.xto3.16.x/  # Source code of the classic Xen front-end driver, which works well with SLES 12 SP and openSUSE 13.2.
| |-- uvp-pvops_xen_driver-2.6.32to4.0.x/     # Source code of the pvops front-end driver, which works well with CentOS/RHEL 6.x, Debian 8, and Fedora 22.
| |-- others/       # Source code of the classic Xen front-end driver, which works well with CentOS/RHEL 4.x/5.x, Fedora 9/12, SLES 10 SP, and Ubuntu 8/12.
|-- version.ini      # Version information of UVP Tools source code.


Installing UVP Tools
  - Obtain the UVP Tools source code package. Save the UVP Tools source code package to a directory on the Linux VM where UVP Tools will be installed, and unpack the UVP Tools source code package. Be sure that you have the permission to access this directory.
    - If the downloaded UVP Tools source code package is an official release, run the following command: 
        tar -xzf UVP-Tools-2.2.0.xxx.tar.gz
        Or
        gunzip UVP-Tools-2.2.0.xxx.zip
    - If the downloaded UVP Tools source code package is a source code package of the master branch, run the following command: 
        gunzip UVP-Tools-master.zip

	
    The Linux VM where UVP Tools will be installed must come with gcc, make, libc, and kernel-devel. For simplicity purposes, the Linux VM where UVP Tools will be installed is referred to as the Linux VM.

  - Go to the uvp-monitor directory. Run the following command to build uvp-monitor:
        make
        Note: When uvp-monitor is being built, the make tool automatically downloads the xen-4.1.2.tar.gz source code package from the Internet. If the download fails, uvp-monitor cannot be successfully built. If the Linux VM cannot access the Internet, manually obtain the xen-4.1.2.tar.gz source code package, and save it to the uvp-monitor directory.   
    
    If uvp-monitor needs to be installed, run the following command:
        make install

  - Build the Xen front-end driver. Take SLES 11 SP3 x86_64 as an example.
    - Go to the uvp-xenpv/uvp-classic_xen_driver-2.6.32to3.0.x directory.
    - Run the following command to build the Xen front-end driver:
        make KERNDIR="/lib/modules/$(uname -r)/build" BUILDKERNEL=$(uname -r)
  
    After the Xen front-end driver is successfully built, run the following command to install the Xen front-end driver:
        make KERNDIR="/lib/modules/$(uname -r)/build" BUILDKERNEL=$(uname -r) modules_install
        Note: This operation may replace the Xen front-end driver provided by the OS vendor with the one provided by UVP Tools. 

    If the Xen front-end driver used by the Linux VM is not provided by UVP Tools, uninstall it. Take the SLES 11 SP3 x86_64 as an example. 
    - Run the following command to check whether the Linux VM is armed with the Xen front-end driver provided by the OS vendor. 
        rpm -qa | grep xen-kmp
    - If a command output is returned, the Linux VM is armed with the Xen front-end driver provided by the OS vendor. Then, run the following command to uninstall the Xen front-end driver: 
        rpm -e xen-kmp-default-4.2.2_04_3.0.76_0.11-0.7.5
        Note: "xen-kmp-default-4.2.2_04_3.0.76_0.11-0.7.5" is merely an example. Replace it with the Xen front-end driver installed on the Linux VM. 

  Alternatively, you can use the build_tools script to build a UVP Tools installation package for the Linux VM. Take SLES 11 SP3 x86_64 as an example. 
  - After the UVP Tools source code package is unpacked, run the following command in the UVP-Tools-2.2.0.xxx directory to build the UVP Tools installation package: 
        make all
  A directory named uvp-tools-linux-2.2.0.xxx is generated in the current directory after the make all command is run. Go to the uvp-tools-linux-2.2.0.xxx directory and run the following command to execute the UVP Tools installation script install. 
        ./install
    Note: If you want to know what the install script has done, run the sh -x install command to print script execution information, or read through the script. 


What can you do if a problem is encountered?
  1. Briefly describe your problem. An one-line summary is recommended.
  2. Describe your problem in detail, including environments, log information, and automatic handling of the problem.
  3. Send the mail to uvptools@huawei.com for further assistance.


