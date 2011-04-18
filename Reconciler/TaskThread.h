#pragma once
#include <queue>
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>

//generically stores calls to functions, similar to boost::bind
//boost::bind copied objects about a dozen times, so I made this one, copies only once

class TaskThread
{
public:

	class BindBase
	{
		public:
			virtual void run( ) = 0;
			virtual ~BindBase( ) = 0;
	};

	typedef boost::shared_ptr< BindBase > BindBasePtr;

private: // ..............private internal types...............
	// -- no parameters --

	template < typename FuncPtr >
	class Bind0 : public BindBase
	{
		public:
			Bind0( FuncPtr func ) : m_func( func ) { }
			inline void run( ) { m_func( ); }
		private:
			FuncPtr m_func;
	};

	// -- one parameter --

	template < typename A, typename FuncPtr >
		class Bind1 : public BindBase
	{
		public:
			Bind1( FuncPtr f, const A & a ) : m_f( f ), m_a( a ) { }
			inline void run( ) { m_f( m_a ); }
		private:
			FuncPtr m_f;
			A m_a;
	};

	// -- no parameters, class member --

	template < typename CLS, typename FuncPtr >
		class Bind0Cls : public BindBase
	{
		public:
			Bind0Cls( CLS * cls, FuncPtr func ) : m_cls( cls ), m_func( func ) { }
			inline void run( ) { ((*m_cls).*m_func)( ); }

		private:
			CLS * m_cls;
			FuncPtr m_func;
	};

	// -- one parameter, class member --

	template < typename CLS, typename A, typename FuncPtr >
		class Bind1Cls : public BindBase
	{
		public:
			Bind1Cls( CLS * cls, FuncPtr f, const A & a ) : m_cls( cls ), m_f( f ) , m_a( a ) { }
			inline void run( ) { ((*m_cls).*m_f)( m_a ); }
		private:
			CLS * m_cls;
			FuncPtr m_f;
			A m_a;
	};

	// -- two parameters, class member --

	template < typename CLS, typename A, typename B, typename FuncPtr >
		class Bind2Cls : public BindBase
	{
		public:
			Bind2Cls( CLS * cls, FuncPtr f, const A & a, const B & b ) : m_cls( cls ), m_f( f ) , m_a( a ), m_b( b ) { }
			inline void run( ) { ((*m_cls).*m_f)( m_a, m_b ); }
		private:
			CLS * m_cls;
			FuncPtr m_f;
			A m_a;
			B m_b;
	};

	// -- three parameters, class member --

	template < typename CLS, typename A, typename B, typename C, typename FuncPtr >
		class Bind3Cls : public BindBase
	{
		public:
			Bind3Cls( CLS * cls, FuncPtr f, const A & a, const B & b, const C & c ) : m_cls( cls ), m_f( f ) , m_a( a ), m_b( b ), m_c( c ) { }
			inline void run( ) { ((*m_cls).*m_f)( m_a, m_b, m_c ); }
		private:
			CLS * m_cls;
			FuncPtr m_f;
			A m_a;
			B m_b;
			C m_c;
	};

	// -- four parameters, class member --

	template < typename CLS, typename A, typename B, typename C, typename D, typename FuncPtr >
		class Bind4Cls : public BindBase
	{
		public:
			Bind4Cls( CLS * cls, FuncPtr f, const A & a, const B & b, const C & c, const D & d ) : m_cls( cls ), m_f( f ) , m_a( a ), m_b( b ), m_c( c ), m_d( d ) { }
			inline void run( ) { ((*m_cls).*m_f)( m_a, m_b, m_c, m_d ); }
		private:
			CLS * m_cls;
			FuncPtr m_f;
			A m_a;
			B m_b;
			C m_c;
			D m_d;
	};

	// -- five parameters, class member --

	template < typename CLS, typename A, typename B, typename C, typename D, typename E, typename FuncPtr >
		class Bind5Cls : public BindBase
	{
		public:
			Bind5Cls( CLS * cls, FuncPtr f, const A & a, const B & b, const C & c, const D & d, const E & e ) : m_cls( cls ), m_f( f ) , m_a( a ), m_b( b ), m_c( c ), m_d( d ), m_e( e ) { }
			inline void run( ) { ((*m_cls).*m_f)( m_a, m_b, m_c, m_d, m_e ); }
		private:
			CLS * m_cls;
			FuncPtr m_f;
			A m_a;
			B m_b;
			C m_c;
			D m_d;
			E m_e;
	};

public:

	template < typename F >
	static BindBasePtr bind( F f ) 
	{ return BindBasePtr( new Bind0< F >( f ) ); }

	template < typename A, typename F >
	static BindBasePtr bind( F f , const A & a ) 
	{ return BindBasePtr( new Bind1< A, F >( f, a ) ); }

	template < typename CLS, typename F >
	static BindBasePtr bind( CLS * cls, F f ) 
	{ return BindBasePtr( new Bind0Cls< CLS, F >( cls, f ) ); }

	template < typename CLS, typename A, typename F >
	static BindBasePtr bind( CLS * cls, F f , const A & a ) 
	{ return BindBasePtr( new Bind1Cls< CLS, A, F >( cls, f, a ) ); }

	template < typename CLS, typename A, typename B, typename F >
	static BindBasePtr bind( CLS * cls, F f , const A & a, const B & b )
	{ return BindBasePtr( new Bind2Cls< CLS, A, B, F >( cls, f, a, b ) ); }

	template < typename CLS, typename A, typename B, typename C, typename F >
	static BindBasePtr bind( CLS * cls, F f , const A & a, const B & b, const C & c ) 
	{ return BindBasePtr( new Bind3Cls< CLS, A, B, C, F >( cls, f, a, b, c ) ); }

	template < typename CLS, typename A, typename B, typename C, typename D, typename F >
	static BindBasePtr bind( CLS * cls, F f , const A & a, const B & b, const C & c, const D & d ) 
	{ return BindBasePtr( new Bind4Cls< CLS, A, B, C, D, F >( cls, f, a, b, c, d ) ); }

	template < typename CLS, typename A, typename B, typename C, typename D, typename F, typename E >
	static BindBasePtr bind( CLS * cls, F f , const A & a, const B & b, const C & c, const D & d, const E & e ) 
	{ return BindBasePtr( new Bind5Cls< CLS, A, B, C, D, E, F >( cls, f, a, b, c, d, e ) ); }

public: //............ interface to this class .......................
	TaskThread( BindBasePtr exception_handler ) 
		: m_exception_handler( exception_handler )
		, m_thread( std::mem_fun( &TaskThread::ThreadProc ), this ) //thread is running now
	{ 
		// don't put anything here that you want the thread to have,
		// the thread is already running :)
	}

	~TaskThread( ) 
	{ 
		//Queue( bind( ThrowInterrupt ) ); //doing it this way ensures we exhaust the queue
		m_thread.interrupt( ); //doing it this way allows us to break out of sleeps and such
		m_thread.join( );
	}

	void Queue( BindBasePtr event )
	{
		boost::lock_guard< boost::mutex > lock( m_lock );
		m_queue.push( event );
		m_condvar.notify_one( );
	}

private:

	static void ThrowInterrupt( ) { throw boost::thread_interrupted( ); }

	static void NoThrowErrorPrint( const char * what ) throw( )
	{
#ifdef _WIN32
		MessageBox( NULL, "what", "Exception Handler Error (taskthread dying)", MB_ICONEXCLAMATION );
#else
		std::cout << "Exception Handler Error (taskthread dying): " << what << std::endl;
#endif
	}

	void ThreadProc( )
	{
		try
		{
			while( true )
			{
				BindBasePtr event; // deletes its pointer when it's destroyed
				{
					boost::unique_lock< boost::mutex > lock( m_lock );

					while( m_queue.empty( ) )
						m_condvar.wait( lock ); // may throw boost::thread_interrupted

					event = m_queue.front( );
					m_queue.pop( );
				}

				try { event->run( ); } // may throw boost::thread_interrupted
				catch( boost::thread_interrupted & ) { throw; }
				catch( ... ) { m_exception_handler->run( ); }
			}
		}
		catch( boost::thread_interrupted & ) { }
		catch( std::exception & e ) { NoThrowErrorPrint( e.what( ) ); }
		catch( ... ) { NoThrowErrorPrint( "unknown exception..." ); }
	}

	boost::mutex m_lock;
	boost::condition_variable m_condvar;
	std::queue< BindBasePtr > m_queue;
	BindBasePtr m_exception_handler;

	// make sure thread is constructed LAST as it will start another thread...
	// and in fact it should be a ptr that I construct ONLY when I'm done
	// initializing this class, but my constructor can be fit into just initializer
	// lists so this is okay to have as a member variable still
	boost::thread m_thread; 
};

inline TaskThread::BindBase::~BindBase( ) { } //may be better than if it were defined above
