/** \brief @file
 * @brief Main include file
 *
 * Contains declarations of all types and functions you need when using
 * LibSingleInstance.
 */
#ifndef SINGLEINSTANCE_H
#define SINGLEINSTANCE_H

#ifdef __cplusplus

#include <stdexcept>

extern "C" {
#endif

#ifdef __DOXYGEN__
/** \brief Opaque communication context
 *
 * This structure is constructed using singleInstanceCreate() and should be
 * treated as opaque identifier. The structure content differs on different
 * platforms.
 */
typedef struct SingleInstanceHandle {} SingleInstanceHandle;
#endif

/** \brief Checks if other instance is running.
 *
 * This function ensures all needed structures are created, checks whether
 * another instance is running and passes arguments to it if it is, or creates
 * communication context (pointed to by struct SingleInstanceHandle *) used
 * to receive arguments from other instances.
 *
 * \param appName Unique name of your application.
 *
 * \param argc Argument count (as given by main()). If you pass 0 no arguments
 *             will be sent.
 *
 * \param argv Pointer to arguments (as given by main()). In case argc is 0,
 *             this may be NULL
 *
 * \param error In case of error, *error (if error isn't NULL) will be filled
 *              with pointer to error message. Message is staticaly allocated.
 *
 * \return Non-null handle if it's the first instance. You may use this handle
 *         in other singleInstance...() calls. Don't forget to call
 *         singleInstanceDestroy to keep your application clean from leaks!
 *
 *         Null if another instance of application is running (*error will be
 *         NULL) or error occured (*error will point to error message).
 */
struct SingleInstanceHandle *singleInstanceCreate(const char *appName, int argc, char **argv, char const **error);

/** \brief Frees up resources taken by SingleInstanceHandle and unlocks the lock
 *
 * This function should be called at exit of application. It will free memory,
 * close file descriptors and cause other instance to know there are no
 * other instances. In case this function is not called (program killed, power
 * failure...) lock is unlocked by operating system. Thus call to this function
 * is not necessary but it will prevent leak analyzers (such as valgrind) from
 * spamming with leak messages.
 *
 * \param handle Handle returned by singleInstanceCreate()
 */
void singleInstanceDestroy(struct SingleInstanceHandle *handle);

/** \brief Checks whether another instance tried to run and retrieves it's arguments
 *
 * You may use this function to receive arguments from other instances. You
 * should periodically call this function because other instances may be
 * blocked. You may create another thread and call this function in infinite
 * loop with non-zero wait.
 *
 * After receiving and processing of arguments, you must call
 * singleInstancePop() which will free the arguments and remove information
 * about the new instance. Until then, this function returns immediately with
 * the same results as previous call.
 *
 * \param handle Handle returned by singleInstanceCreate()
 *
 * \param argc Pointer to int where argument count should be stored
 *
 * \param argv Pointer to char ** where arguments should be stored
 *
 * \param wait If nonzero, this function will block until some instance sends
 *             arguments. If zero, this function returns immediately (but may
 *             block if other instance is sending too slowly)
 *
 * \return 1 if another instance was executed. argc and argv are filled in this case.
 *         0 if there are no other instances or waiting was interrupted by
 *         singleInstanceStopWait().
 */
int singleInstanceCheck(struct SingleInstanceHandle *handle, int *argc, char ***argv, int wait);

/** \brief Interrupts waiting of singleInstanceCheck()
 *
 * In case there is singleInstanceCheck() waiting for another instance, it will
 * stop waiting and return immediately with return value 0. If no thread waits,
 * future wait will return immediately with return value 0. This may be used
 * to stop other thread if stop is requested. (e. g. via signal; it IS safe to
 * call this function from POSIX signal handler)
 *
 * This function may be called safely from any thread.
 *
 * \param handle Handle returned by singleInstanceCreate()
 */
void singleInstanceStopWait(struct SingleInstanceHandle *handle);

/** \brief Removes information about first instance
 *
 * After arguments received from singleInstanceCheck() are processed and
 * information is no longer needed, this function must be called to remove it
 * and free allocated memory. singleInstanceCheck() will return same old values
 * until singleInstanceStopWait() is called.
 *
 * \param handle Handle returned by singleInstanceCreate()
 */
void singleInstancePop(struct SingleInstanceHandle *handle);

#ifdef __cplusplus
}

namespace LibSingleInstance {

/** \brief C++ wrapper for struct SingleInstanceHandle *
 *
 * If you program in C++ use this wrapper instead of raw C functions. You will
 * avoid some problems and the code will look better.
 */
class SingleInstance {
	public:
		/** \brief Constructor. Wrapper for singleInstanceCreate().
		 *
		 * This function checks whether other instance is running (throws Exists
		 * if it is; Error in case of error) and passes arguments to it. If instance
		 * is not running, communication context is created.
		 */
		SingleInstance(const std::string &appName, int argc = 0, char **argv = NULL);

		/** \brief Destructor. Wrapper for singleInstanceDestroy().
		 *
		 * This function calls singleInstanceDestroy() causing
		 * resources to free.
		 */
		~SingleInstance();

		/** \brief Checks for running instances.
		 * Wrapper for singleInstanceCheck()
		 *
		 * As in case of singleInstanceCheck() returns 1 if other
		 * instance is running or 0 if not (or call was interrupted)
		 */
		int check(int *argc, char ***argv, bool wait);

		/** \brief Frees information about other instance. Wrapper for
		 * singleInstancePop().
		 */
		void pop();

		/** \brief Interrupts waiting of check().
		 * Wrapper for singleInstanceStopWait().
		 */
		void stop();

		/** \brief Exception thrown when other instance is running */
		class Exists : public std::runtime_error {
			public:
				/** \brief Constructor */
				Exists();
				/** \brief Destructor */
				~Exists() throw();
		};

		/** \brief Exception thrown when error occurs */
		class Error : public std::runtime_error {
			public:
				/** \brief Constructor 
				 * \param msg Human-readable error message
				 */
				Error(const char *msg);
				/** \brief Destructor */
				~Error() throw();
		};
	private:
		struct SingleInstanceHandle *mHandle;

		// Copying disabled
		SingleInstance(const SingleInstance &);
		SingleInstance &operator=(const SingleInstance &);
};

#ifndef SINGLEINSTANCE_NO_INLINES
inline SingleInstance::SingleInstance(const std::string &appName, int argc, char **argv) {
			const char *err;
			mHandle = singleInstanceCreate(appName.c_str(), argc, argv, &err);
			if(err) throw Error(err);
			if(!mHandle) throw Exists();
}

inline SingleInstance::~SingleInstance() {
	singleInstanceDestroy(mHandle);
}

inline int SingleInstance::check(int *argc, char ***argv, bool wait) {
	return singleInstanceCheck(mHandle, argc, argv, wait);
}

inline void SingleInstance::pop() {
	singleInstancePop(mHandle);
}

inline void SingleInstance::stop() {
	singleInstanceStopWait(mHandle);
}

inline SingleInstance::Exists::Exists() : std::runtime_error("Instance already running") {}

inline SingleInstance::Exists::~Exists() throw() {}

inline SingleInstance::Error::Error(const char *msg) : std::runtime_error(msg) {}

inline SingleInstance::Error::~Error() throw() {}

#endif // SINGLEINSTANCE_NO_INLINES

} // namespace

#endif // __cplusplus
#endif // SINGLEINSTANCE_H
