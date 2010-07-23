// jsobjtests.cpp - Tests for jsobj.{h,cpp} code
//

/**
 *    Copyright (C) 2008 10gen Inc.
 *
 *    This program is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "pch.h"
#include "../db/jsobj.h"
#include "../db/jsobjmanipulator.h"
#include "../db/json.h"
#include "../db/repl.h"
#include "../db/extsort.h"

#include "dbtests.h"

namespace JsobjTests {
    class BufBuilderBasic {
    public:
        void run() {
            BufBuilder b( 0 );
            b.appendStr( "foo" );
            ASSERT_EQUALS( 4, b.len() );
            ASSERT( strcmp( "foo", b.buf() ) == 0 );
        }
    };

    class BSONElementBasic {
    public:
        void run() {
            ASSERT_EQUALS( 1, BSONElement().size() );
        }
    };

    namespace BSONObjTests {
        class Create {
        public:
            void run() {
                BSONObj b;
                ASSERT_EQUALS( 0, b.nFields() );
            }
        };

        class Base {
        protected:
            static BSONObj basic( const char *name, int val ) {
                BSONObjBuilder b;
                b.append( name, val );
                return b.obj();
            }
            static BSONObj basic( const char *name, vector< int > val ) {
                BSONObjBuilder b;
                b.append( name, val );
                return b.obj();
            }
            template< class T >
            static BSONObj basic( const char *name, T val ) {
                BSONObjBuilder b;
                b.append( name, val );
                return b.obj();
            }
        };

        class WoCompareBasic : public Base {
        public:
            void run() {
                ASSERT( basic( "a", 1 ).woCompare( basic( "a", 1 ) ) == 0 );
                ASSERT( basic( "a", 2 ).woCompare( basic( "a", 1 ) ) > 0 );
                ASSERT( basic( "a", 1 ).woCompare( basic( "a", 2 ) ) < 0 );
                // field name comparison
                ASSERT( basic( "a", 1 ).woCompare( basic( "b", 1 ) ) < 0 );
            }
        };

        class NumericCompareBasic : public Base {
        public:
            void run() {
                ASSERT( basic( "a", 1 ).woCompare( basic( "a", 1.0 ) ) == 0 );
            }
        };

        class WoCompareEmbeddedObject : public Base {
        public:
            void run() {
                ASSERT( basic( "a", basic( "b", 1 ) ).woCompare
                        ( basic( "a", basic( "b", 1.0 ) ) ) == 0 );
                ASSERT( basic( "a", basic( "b", 1 ) ).woCompare
                        ( basic( "a", basic( "b", 2 ) ) ) < 0 );
            }
        };

        class WoCompareEmbeddedArray : public Base {
        public:
            void run() {
                vector< int > i;
                i.push_back( 1 );
                i.push_back( 2 );
                vector< double > d;
                d.push_back( 1 );
                d.push_back( 2 );
                ASSERT( basic( "a", i ).woCompare( basic( "a", d ) ) == 0 );

                vector< int > j;
                j.push_back( 1 );
                j.push_back( 3 );
                ASSERT( basic( "a", i ).woCompare( basic( "a", j ) ) < 0 );
            }
        };

        class WoCompareOrdered : public Base {
        public:
            void run() {
                ASSERT( basic( "a", 1 ).woCompare( basic( "a", 1 ), basic( "a", 1 ) ) == 0 );
                ASSERT( basic( "a", 2 ).woCompare( basic( "a", 1 ), basic( "a", 1 ) ) > 0 );
                ASSERT( basic( "a", 1 ).woCompare( basic( "a", 2 ), basic( "a", 1 ) ) < 0 );
                ASSERT( basic( "a", 1 ).woCompare( basic( "a", 1 ), basic( "a", -1 ) ) == 0 );
                ASSERT( basic( "a", 2 ).woCompare( basic( "a", 1 ), basic( "a", -1 ) ) < 0 );
                ASSERT( basic( "a", 1 ).woCompare( basic( "a", 2 ), basic( "a", -1 ) ) > 0 );
            }
        };

        class WoCompareDifferentLength : public Base {
        public:
            void run() {
                ASSERT( BSON( "a" << 1 ).woCompare( BSON( "a" << 1 << "b" << 1 ) ) < 0 );
                ASSERT( BSON( "a" << 1 << "b" << 1 ).woCompare( BSON( "a" << 1 ) ) > 0 );
            }
        };

        class WoSortOrder : public Base {
        public:
            void run() {
                ASSERT( BSON( "a" << 1 ).woSortOrder( BSON( "a" << 2 ), BSON( "b" << 1 << "a" << 1 ) ) < 0 );
                ASSERT( fromjson( "{a:null}" ).woSortOrder( BSON( "b" << 1 ), BSON( "a" << 1 ) ) == 0 );
            }
        };

        class MultiKeySortOrder : public Base {
        public:
            void run(){
                ASSERT( BSON( "x" << "a" ).woCompare( BSON( "x" << "b" ) ) < 0 );
                ASSERT( BSON( "x" << "b" ).woCompare( BSON( "x" << "a" ) ) > 0 );

                ASSERT( BSON( "x" << "a" << "y" << "a" ).woCompare( BSON( "x" << "a" << "y" << "b" ) ) < 0 );
                ASSERT( BSON( "x" << "a" << "y" << "a" ).woCompare( BSON( "x" << "b" << "y" << "a" ) ) < 0 );
                ASSERT( BSON( "x" << "a" << "y" << "a" ).woCompare( BSON( "x" << "b" ) ) < 0 );

                ASSERT( BSON( "x" << "c" ).woCompare( BSON( "x" << "b" << "y" << "h" ) ) > 0 );
                ASSERT( BSON( "x" << "b" << "y" << "b" ).woCompare( BSON( "x" << "c" ) ) < 0 );

                BSONObj key = BSON( "x" << 1 << "y" << 1 );

                ASSERT( BSON( "x" << "c" ).woSortOrder( BSON( "x" << "b" << "y" << "h" ) , key ) > 0 );
                ASSERT( BSON( "x" << "b" << "y" << "b" ).woCompare( BSON( "x" << "c" ) , key ) < 0 );

                key = BSON( "" << 1 << "" << 1 );

                ASSERT( BSON( "" << "c" ).woSortOrder( BSON( "" << "b" << "" << "h" ) , key ) > 0 );
                ASSERT( BSON( "" << "b" << "" << "b" ).woCompare( BSON( "" << "c" ) , key ) < 0 );

                {
                    BSONObjBuilder b;
                    b.append( "" , "c" );
                    b.appendNull( "" );
                    BSONObj o = b.obj();
                    ASSERT( o.woSortOrder( BSON( "" << "b" << "" << "h" ) , key ) > 0 );
                    ASSERT( BSON( "" << "b" << "" << "h" ).woSortOrder( o , key ) < 0 );

                }


                ASSERT( BSON( "" << "a" ).woCompare( BSON( "" << "a" << "" << "c" ) ) < 0 );
                {
                    BSONObjBuilder b;
                    b.append( "" , "a" );
                    b.appendNull( "" );
                    ASSERT(  b.obj().woCompare( BSON( "" << "a" << "" << "c" ) ) < 0 ); // SERVER-282
                }

            }
        };

        class TimestampTest : public Base {
        public:
            void run() {
                BSONObjBuilder b;
                b.appendTimestamp( "a" );
                BSONObj o = b.done();
                o.toString();
                ASSERT( o.valid() );
                ASSERT_EQUALS( Timestamp, o.getField( "a" ).type() );
                BSONObjIterator i( o );
                ASSERT( i.moreWithEOO() );
                ASSERT( i.more() );

                BSONElement e = i.next();
                ASSERT_EQUALS( Timestamp, e.type() );
                ASSERT( i.moreWithEOO() );
                ASSERT( ! i.more() );

                e = i.next();
                ASSERT( e.eoo() );

                OpTime before = OpTime::now();
                BSONElementManipulator( o.firstElement() ).initTimestamp();
                OpTime after = OpTime::now();

                OpTime test = OpTime( o.firstElement().date() );
                ASSERT( before < test && test < after );

                BSONElementManipulator( o.firstElement() ).initTimestamp();
                test = OpTime( o.firstElement().date() );
                ASSERT( before < test && test < after );
            }
        };

        class Nan : public Base {
        public:
            void run() {
                double inf = numeric_limits< double >::infinity();
                double nan = numeric_limits< double >::quiet_NaN();
                double nan2 = numeric_limits< double >::signaling_NaN();

                ASSERT( BSON( "a" << inf ).woCompare( BSON( "a" << inf ) ) == 0 );
                ASSERT( BSON( "a" << inf ).woCompare( BSON( "a" << 1 ) ) < 0 );
                ASSERT( BSON( "a" << 1 ).woCompare( BSON( "a" << inf ) ) > 0 );

                ASSERT( BSON( "a" << nan ).woCompare( BSON( "a" << nan ) ) == 0 );
                ASSERT( BSON( "a" << nan ).woCompare( BSON( "a" << 1 ) ) < 0 );
                ASSERT( BSON( "a" << 1 ).woCompare( BSON( "a" << nan ) ) > 0 );

                ASSERT( BSON( "a" << nan2 ).woCompare( BSON( "a" << nan2 ) ) == 0 );
                ASSERT( BSON( "a" << nan2 ).woCompare( BSON( "a" << 1 ) ) < 0 );
                ASSERT( BSON( "a" << 1 ).woCompare( BSON( "a" << nan2 ) ) > 0 );

                ASSERT( BSON( "a" << inf ).woCompare( BSON( "a" << nan ) ) == 0 );
                ASSERT( BSON( "a" << inf ).woCompare( BSON( "a" << nan2 ) ) == 0 );
                ASSERT( BSON( "a" << nan ).woCompare( BSON( "a" << nan2 ) ) == 0 );
            }
        };

        class AsTempObj{
        public:
            void run(){
                {
                    BSONObjBuilder bb;
                    bb << "a" << 1;
                    BSONObj tmp = bb.asTempObj();
                    ASSERT(tmp.objsize() == 4+(1+2+4)+1);
                    ASSERT(tmp.valid());
                    ASSERT(tmp.hasField("a"));
                    ASSERT(!tmp.hasField("b"));
                    ASSERT(tmp == BSON("a" << 1));
                    
                    bb << "b" << 2;
                    BSONObj obj = bb.obj();
                    ASSERT_EQUALS(obj.objsize() , 4+(1+2+4)+(1+2+4)+1);
                    ASSERT(obj.valid());
                    ASSERT(obj.hasField("a"));
                    ASSERT(obj.hasField("b"));
                    ASSERT(obj == BSON("a" << 1 << "b" << 2));
                }
                {
                    BSONObjBuilder bb;
                    bb << "a" << GT << 1;
                    BSONObj tmp = bb.asTempObj();
                    ASSERT(tmp.objsize() == 4+(1+2+(4+1+4+4+1))+1);
                    ASSERT(tmp.valid());
                    ASSERT(tmp.hasField("a"));
                    ASSERT(!tmp.hasField("b"));
                    ASSERT(tmp == BSON("a" << BSON("$gt" << 1)));
                    
                    bb << "b" << LT << 2;
                    BSONObj obj = bb.obj();
                    ASSERT(obj.objsize() == 4+(1+2+(4+1+4+4+1))+(1+2+(4+1+4+4+1))+1);
                    ASSERT(obj.valid());
                    ASSERT(obj.hasField("a"));
                    ASSERT(obj.hasField("b"));
                    ASSERT(obj == BSON("a" << BSON("$gt" << 1)
                                    << "b" << BSON("$lt" << 2)));
                }
                {
                    BSONObjBuilder bb(32);
                    bb << "a" << 1;
                    BSONObj tmp = bb.asTempObj();
                    ASSERT(tmp.objsize() == 4+(1+2+4)+1);
                    ASSERT(tmp.valid());
                    ASSERT(tmp.hasField("a"));
                    ASSERT(!tmp.hasField("b"));
                    ASSERT(tmp == BSON("a" << 1));

                    //force a realloc
                    BSONArrayBuilder arr;
                    for (int i=0; i < 10000; i++){
                        arr << i;
                    }
                    bb << "b" << arr.arr();
                    BSONObj obj = bb.obj();
                    ASSERT(obj.valid());
                    ASSERT(obj.hasField("a"));
                    ASSERT(obj.hasField("b"));
                    ASSERT_NOT_EQUALS(obj.objdata() , tmp.objdata());
                }
            }
        };

        struct AppendIntOrLL{
            void run(){
                const long long billion = 1000*1000*1000;
                BSONObjBuilder b;
                b.appendIntOrLL("i1",  1);
                b.appendIntOrLL("i2", -1);
                b.appendIntOrLL("i3",  1*billion);
                b.appendIntOrLL("i4", -1*billion);

                b.appendIntOrLL("L1",  2*billion);
                b.appendIntOrLL("L2", -2*billion);
                b.appendIntOrLL("L3",  4*billion);
                b.appendIntOrLL("L4", -4*billion);
                b.appendIntOrLL("L5",  16*billion);
                b.appendIntOrLL("L6", -16*billion);

                BSONObj o = b.obj();

                ASSERT(o["i1"].type() == NumberInt);
                ASSERT(o["i1"].number() == 1);
                ASSERT(o["i2"].type() == NumberInt);
                ASSERT(o["i2"].number() == -1);
                ASSERT(o["i3"].type() == NumberInt);
                ASSERT(o["i3"].number() == 1*billion);
                ASSERT(o["i4"].type() == NumberInt);
                ASSERT(o["i4"].number() == -1*billion);

                ASSERT(o["L1"].type() == NumberLong);
                ASSERT(o["L1"].number() == 2*billion);
                ASSERT(o["L2"].type() == NumberLong);
                ASSERT(o["L2"].number() == -2*billion);
                ASSERT(o["L3"].type() == NumberLong);
                ASSERT(o["L3"].number() == 4*billion);
                ASSERT(o["L4"].type() == NumberLong);
                ASSERT(o["L4"].number() == -4*billion);
                ASSERT(o["L5"].type() == NumberLong);
                ASSERT(o["L5"].number() == 16*billion);
                ASSERT(o["L6"].type() == NumberLong);
                ASSERT(o["L6"].number() == -16*billion);
            }
        };

        struct AppendNumber {
            void run(){
                BSONObjBuilder b;
                b.appendNumber( "a" , 5 );
                b.appendNumber( "b" , 5.5 );
                b.appendNumber( "c" , (1024LL*1024*1024)-1 );
                b.appendNumber( "d" , (1024LL*1024*1024*1024)-1 );
                b.appendNumber( "e" , 1024LL*1024*1024*1024*1024*1024 );
                
                BSONObj o = b.obj();
                
                ASSERT( o["a"].type() == NumberInt );
                ASSERT( o["b"].type() == NumberDouble );
                ASSERT( o["c"].type() == NumberInt );
                ASSERT( o["d"].type() == NumberDouble );
                ASSERT( o["e"].type() == NumberLong );

            }
        };
        
        class ToStringArray {
        public:
            void run() {
                string spec = "{ a: [ \"a\", \"b\" ] }";
                ASSERT_EQUALS( spec, fromjson( spec ).toString() );
            }
        };

        class NullString {
        public:
            void run() {
                BSONObjBuilder b;
                b.append("a", "a\0b", 4);
                b.append("b", string("a\0b", 3));
                b.appendAs(b.asTempObj()["a"], "c");
                BSONObj o = b.obj();

                stringstream ss;
                ss << 'a' << '\0' << 'b';

                ASSERT_EQUALS(o["a"].valuestrsize(), 3+1);
                ASSERT_EQUALS(o["a"].str(), ss.str());

                ASSERT_EQUALS(o["b"].valuestrsize(), 3+1);
                ASSERT_EQUALS(o["b"].str(), ss.str());

                ASSERT_EQUALS(o["c"].valuestrsize(), 3+1);
                ASSERT_EQUALS(o["c"].str(), ss.str());
            }

        };

        namespace Validation {

            class Base {
            public:
                virtual ~Base() {}
                void run() {
                    ASSERT( valid().valid() );
                    ASSERT( !invalid().valid() );
                }
            protected:
                virtual BSONObj valid() const { return BSONObj(); }
                virtual BSONObj invalid() const { return BSONObj(); }
                static char get( const BSONObj &o, int i ) {
                    return o.objdata()[ i ];
                }
                static void set( BSONObj &o, int i, char c ) {
                    const_cast< char * >( o.objdata() )[ i ] = c;
                }
            };

            class BadType : public Base {
                BSONObj valid() const {
                    return fromjson( "{\"a\":1}" );
                }
                BSONObj invalid() const {
                    BSONObj ret = valid();
                    set( ret, 4, 50 );
                    return ret;
                }
            };

            class EooBeforeEnd : public Base {
                BSONObj valid() const {
                    return fromjson( "{\"a\":1}" );
                }
                BSONObj invalid() const {
                    BSONObj ret = valid();
                    // (first byte of size)++
                    set( ret, 0, get( ret, 0 ) + 1 );
                    // re-read size for BSONObj::details
                    return ret.copy();
                }
            };

            class Undefined : public Base {
            public:
                void run() {
                    BSONObjBuilder b;
                    b.appendNull( "a" );
                    BSONObj o = b.done();
                    set( o, 4, mongo::Undefined );
                    ASSERT( o.valid() );
                }
            };

            class TotalSizeTooSmall : public Base {
                BSONObj valid() const {
                    return fromjson( "{\"a\":1}" );
                }
                BSONObj invalid() const {
                    BSONObj ret = valid();
                    // (first byte of size)--
                    set( ret, 0, get( ret, 0 ) - 1 );
                    // re-read size for BSONObj::details
                    return ret.copy();
                }
            };

            class EooMissing : public Base {
                BSONObj valid() const {
                    return fromjson( "{\"a\":1}" );
                }
                BSONObj invalid() const {
                    BSONObj ret = valid();
                    set( ret, ret.objsize() - 1, (char) 0xff );
                    // (first byte of size)--
                    set( ret, 0, get( ret, 0 ) - 1 );
                    // re-read size for BSONObj::details
                    return ret.copy();
                }
            };

            class WrongStringSize : public Base {
                BSONObj valid() const {
                    return fromjson( "{\"a\":\"b\"}" );
                }
                BSONObj invalid() const {
                    BSONObj ret = valid();
                    ASSERT_EQUALS( ret.firstElement().valuestr()[0] , 'b' );
                    ASSERT_EQUALS( ret.firstElement().valuestr()[1] , 0 );
                    ((char*)ret.firstElement().valuestr())[1] = 1;
                    return ret.copy();
                }
            };

            class ZeroStringSize : public Base {
                BSONObj valid() const {
                    return fromjson( "{\"a\":\"b\"}" );
                }
                BSONObj invalid() const {
                    BSONObj ret = valid();
                    set( ret, 7, 0 );
                    return ret;
                }
            };

            class NegativeStringSize : public Base {
                BSONObj valid() const {
                    return fromjson( "{\"a\":\"b\"}" );
                }
                BSONObj invalid() const {
                    BSONObj ret = valid();
                    set( ret, 10, -100 );
                    return ret;
                }
            };

            class WrongSubobjectSize : public Base {
                BSONObj valid() const {
                    return fromjson( "{\"a\":{\"b\":1}}" );
                }
                BSONObj invalid() const {
                    BSONObj ret = valid();
                    set( ret, 0, get( ret, 0 ) + 1 );
                    set( ret, 7, get( ret, 7 ) + 1 );
                    return ret.copy();
                }
            };

            class WrongDbrefNsSize : public Base {
                BSONObj valid() const {
                    return fromjson( "{ \"a\": Dbref( \"b\", \"ffffffffffffffffffffffff\" ) }" );
                }
                BSONObj invalid() const {
                    BSONObj ret = valid();
                    set( ret, 0, get( ret, 0 ) + 1 );
                    set( ret, 7, get( ret, 7 ) + 1 );
                    return ret.copy();
                };
            };

            class NoFieldNameEnd : public Base {
                BSONObj valid() const {
                    return fromjson( "{\"a\":1}" );
                }
                BSONObj invalid() const {
                    BSONObj ret = valid();
                    memset( const_cast< char * >( ret.objdata() ) + 5, 0xff, ret.objsize() - 5 );
                    return ret;
                }
            };

            class BadRegex : public Base {
                BSONObj valid() const {
                    return fromjson( "{\"a\":/c/i}" );
                }
                BSONObj invalid() const {
                    BSONObj ret = valid();
                    memset( const_cast< char * >( ret.objdata() ) + 7, 0xff, ret.objsize() - 7 );
                    return ret;
                }
            };

            class BadRegexOptions : public Base {
                BSONObj valid() const {
                    return fromjson( "{\"a\":/c/i}" );
                }
                BSONObj invalid() const {
                    BSONObj ret = valid();
                    memset( const_cast< char * >( ret.objdata() ) + 9, 0xff, ret.objsize() - 9 );
                    return ret;
                }
            };

            class CodeWScopeBase : public Base {
                BSONObj valid() const {
                    BSONObjBuilder b;
                    BSONObjBuilder scope;
                    scope.append( "a", "b" );
                    b.appendCodeWScope( "c", "d", scope.done() );
                    return b.obj();
                }
                BSONObj invalid() const {
                    BSONObj ret = valid();
                    modify( ret );
                    return ret;
                }
            protected:
                virtual void modify( BSONObj &o ) const = 0;
            };

            class CodeWScopeSmallSize : public CodeWScopeBase {
                void modify( BSONObj &o ) const {
                    set( o, 7, 7 );
                }
            };

            class CodeWScopeZeroStrSize : public CodeWScopeBase {
                void modify( BSONObj &o ) const {
                    set( o, 11, 0 );
                }
            };

            class CodeWScopeSmallStrSize : public CodeWScopeBase {
                void modify( BSONObj &o ) const {
                    set( o, 11, 1 );
                }
            };

            class CodeWScopeNoSizeForObj : public CodeWScopeBase {
                void modify( BSONObj &o ) const {
                    set( o, 7, 13 );
                }
            };

            class CodeWScopeSmallObjSize : public CodeWScopeBase {
                void modify( BSONObj &o ) const {
                    set( o, 17, 1 );
                }
            };

            class CodeWScopeBadObject : public CodeWScopeBase {
                void modify( BSONObj &o ) const {
                    set( o, 21, JSTypeMax + 1 );
                }
            };

            class NoSize {
            public:
                NoSize( BSONType type ) : type_( type ) {}
                void run() {
                    const char data[] = { 0x07, 0x00, 0x00, 0x00, char( type_ ), 'a', 0x00 };
                    BSONObj o( data );
                    ASSERT( !o.valid() );
                }
            private:
                BSONType type_;
            };

            // Randomized BSON parsing test.  See if we seg fault.
            class Fuzz {
            public:
                Fuzz( double frequency ) : frequency_( frequency ) {}
                void run() {
                    BSONObj a = fromjson( "{\"a\": 1, \"b\": \"c\"}" );
                    fuzz( a );
                    a.valid();

                    BSONObj b = fromjson( "{\"one\":2, \"two\":5, \"three\": {},"
                                         "\"four\": { \"five\": { \"six\" : 11 } },"
                                         "\"seven\": [ \"a\", \"bb\", \"ccc\", 5 ],"
                                         "\"eight\": Dbref( \"rrr\", \"01234567890123456789aaaa\" ),"
                                         "\"_id\": ObjectId( \"deadbeefdeadbeefdeadbeef\" ),"
                                         "\"nine\": { \"$binary\": \"abc=\", \"$type\": \"00\" },"
                                         "\"ten\": Date( 44 ), \"eleven\": /foooooo/i }" );
                    fuzz( b );
                    b.valid();
                }
            private:
                void fuzz( BSONObj &o ) const {
                    for( int i = 4; i < o.objsize(); ++i )
                        for( unsigned char j = 1; j; j <<= 1 )
                            if ( rand() < int( RAND_MAX * frequency_ ) ) {
                                char *c = const_cast< char * >( o.objdata() ) + i;
                                if ( *c & j )
                                    *c &= ~j;
                                else
                                    *c |= j;
                            }
                }
                double frequency_;
            };

        } // namespace Validation

    } // namespace BSONObjTests

    namespace OIDTests {

        class init1 {
        public:
            void run(){
                OID a;
                OID b;

                a.init();
                b.init();

                ASSERT( a != b );
            }
        };

        class initParse1 {
        public:
            void run(){

                OID a;
                OID b;

                a.init();
                b.init( a.str() );

                ASSERT( a == b );
            }
        };

        class append {
        public:
            void run(){
                BSONObjBuilder b;
                b.appendOID( "a" , 0 );
                b.appendOID( "b" , 0 , false );
                b.appendOID( "c" , 0 , true );
                BSONObj o = b.obj();

                ASSERT( o["a"].__oid().str() == "000000000000000000000000" );
                ASSERT( o["b"].__oid().str() == "000000000000000000000000" );
                ASSERT( o["c"].__oid().str() != "000000000000000000000000" );

            }
        };

        class increasing {
        public:
            BSONObj g(){
                BSONObjBuilder b;
                b.appendOID( "_id" , 0 , true );
                return b.obj();
            }
            void run(){
                BSONObj a = g();
                BSONObj b = g();
                
                ASSERT( a.woCompare( b ) < 0 );
                
                // yes, there is a 1/1000 chance this won't increase time(0) 
                // and therefore inaccurately say the function is behaving
                // buf if its broken, it will fail 999/1000, so i think that's good enough
                sleepsecs( 1 );
                BSONObj c = g();
                ASSERT( a.woCompare( c ) < 0 );
            }
        };

        class ToDate {
        public:
            void run(){
                OID oid;

                {
                    time_t before = ::time(0);
                    oid.init();
                    time_t after = ::time(0);
                    ASSERT( oid.asTimeT() >= before );
                    ASSERT( oid.asTimeT() <= after );
                }

                {
                    Date_t before = jsTime();
                    sleepsecs(1);
                    oid.init();
                    Date_t after = jsTime();
                    ASSERT( oid.asDateT() >= before );
                    ASSERT( oid.asDateT() <= after );
                }
            }
        };

        class FromDate {
        public:
            void run(){
                OID min, oid, max;
                Date_t now = jsTime();
                oid.init(); // slight chance this has different time. If its a problem, can change.
                min.init(now);
                max.init(now, true);

                ASSERT_EQUALS( (unsigned)oid.asTimeT() , now/1000 );
                ASSERT_EQUALS( (unsigned)min.asTimeT() , now/1000 );
                ASSERT_EQUALS( (unsigned)max.asTimeT() , now/1000 );
                ASSERT( BSON("" << min).woCompare( BSON("" << oid) ) < 0  );
                ASSERT( BSON("" << max).woCompare( BSON("" << oid)  )> 0  );
            }
        };
    } // namespace OIDTests


    namespace ValueStreamTests {

        class LabelBase {
        public:
            virtual ~LabelBase() {}
            void run() {
                ASSERT( !expected().woCompare( actual() ) );
            }
        protected:
            virtual BSONObj expected() = 0;
            virtual BSONObj actual() = 0;
        };

        class LabelBasic : public LabelBase {
            BSONObj expected() {
                return BSON( "a" << ( BSON( "$gt" << 1 ) ) );
            }
            BSONObj actual() {
                return BSON( "a" << GT << 1 );
            }
        };

        class LabelShares : public LabelBase {
            BSONObj expected() {
                return BSON( "z" << "q" << "a" << ( BSON( "$gt" << 1 ) ) << "x" << "p" );
            }
            BSONObj actual() {
                return BSON( "z" << "q" << "a" << GT << 1 << "x" << "p" );
            }
        };

        class LabelDouble : public LabelBase {
            BSONObj expected() {
                return BSON( "a" << ( BSON( "$gt" << 1 << "$lte" << "x" ) ) );
            }
            BSONObj actual() {
                return BSON( "a" << GT << 1 << LTE << "x" );
            }
        };

        class LabelDoubleShares : public LabelBase {
            BSONObj expected() {
                return BSON( "z" << "q" << "a" << ( BSON( "$gt" << 1 << "$lte" << "x" ) ) << "x" << "p" );
            }
            BSONObj actual() {
                return BSON( "z" << "q" << "a" << GT << 1 << LTE << "x" << "x" << "p" );
            }
        };

        class LabelSize : public LabelBase {
            BSONObj expected() {
                return BSON( "a" << BSON( "$size" << 4 ) );
            }
            BSONObj actual() {
                return BSON( "a" << mongo::SIZE << 4 );
            }
        };

        class LabelMulti : public LabelBase {
            BSONObj expected() {
                return BSON( "z" << "q"
                            << "a" << BSON( "$gt" << 1 << "$lte" << "x" )
                            << "b" << BSON( "$ne" << 1 << "$ne" << "f" << "$ne" << 22.3 )
                            << "x" << "p" );
            }
            BSONObj actual() {
                return BSON( "z" << "q"
                            << "a" << GT << 1 << LTE << "x"
                            << "b" << NE << 1 << NE << "f" << NE << 22.3
                            << "x" << "p" );
            }
        };
        class LabelishOr : public LabelBase {
            BSONObj expected() {
                return BSON( "$or" << BSON_ARRAY(
                               BSON("a" << BSON( "$gt" << 1 << "$lte" << "x" ))
                            << BSON("b" << BSON( "$ne" << 1 << "$ne" << "f" << "$ne" << 22.3 ))
                            << BSON("x" << "p" )));
            }
            BSONObj actual() {
                return OR( BSON( "a" << GT << 1 << LTE << "x"), 
                           BSON( "b" << NE << 1 << NE << "f" << NE << 22.3),
                           BSON( "x" << "p" ) );
            }
        };

        class Unallowed {
        public:
            void run() {
                ASSERT_EXCEPTION( BSON( GT << 4 ), MsgAssertionException );
                ASSERT_EXCEPTION( BSON( "a" << 1 << GT << 4 ), MsgAssertionException );
            }
        };

        class ElementAppend {
        public:
            void run(){
                BSONObj a = BSON( "a" << 17 );
                BSONObj b = BSON( "b" << a["a"] );
                ASSERT_EQUALS( NumberInt , a["a"].type() );
                ASSERT_EQUALS( NumberInt , b["b"].type() );
                ASSERT_EQUALS( 17 , b["b"].number() );
            }
        };

    } // namespace ValueStreamTests

    class SubObjectBuilder {
    public:
        void run() {
            BSONObjBuilder b1;
            b1.append( "a", "bcd" );
            BSONObjBuilder b2( b1.subobjStart( "foo" ) );
            b2.append( "ggg", 44.0 );
            b2.done();
            b1.append( "f", 10.0 );
            BSONObj ret = b1.done();
            ASSERT( ret.valid() );
            ASSERT( ret.woCompare( fromjson( "{a:'bcd',foo:{ggg:44},f:10}" ) ) == 0 );
        }
    };

    class DateBuilder {
    public:
        void run() {
            BSONObj o = BSON("" << Date_t(1234567890));
            ASSERT( o.firstElement().type() == Date );
            ASSERT( o.firstElement().date() == Date_t(1234567890) );
        }
    };

    class DateNowBuilder {
    public:
        void run() {
            Date_t before = jsTime();
            BSONObj o = BSON("now" << DATENOW);
            Date_t after = jsTime();

            ASSERT( o.valid() );

            BSONElement e = o["now"];
            ASSERT( e.type() == Date );
            ASSERT( e.date() >= before );
            ASSERT( e.date() <= after );
        }
    };

    class TimeTBuilder {
    public:
        void run() {
            Date_t before = jsTime();
            sleepmillis(1);
            time_t now = time(NULL);
            sleepmillis(1);
            Date_t after = jsTime();

            BSONObjBuilder b;
            b.appendTimeT("now", now);
            BSONObj o = b.obj();

            ASSERT( o.valid() );

            BSONElement e = o["now"];
            ASSERT( e.type() == Date );
            ASSERT( e.date()/1000 >= before/1000 );
            ASSERT( e.date()/1000 <= after/1000 );
        }
    };

    class MinMaxElementTest {
    public:

        BSONObj min( int t ){
            BSONObjBuilder b;
            b.appendMinForType( "a" , t );
            return b.obj();
        }

        BSONObj max( int t ){
            BSONObjBuilder b;
            b.appendMaxForType( "a" , t );
            return b.obj();
        }

        void run(){
            for ( int t=1; t<JSTypeMax; t++ ){
                stringstream ss;
                ss << "type: " << t;
                string s = ss.str();
                massert( 10403 ,  s , min( t ).woCompare( max( t ) ) < 0 );
                massert( 10404 ,  s , max( t ).woCompare( min( t ) ) > 0 );
                massert( 10405 ,  s , min( t ).woCompare( min( t ) ) == 0 );
                massert( 10406 ,  s , max( t ).woCompare( max( t ) ) == 0 );
                massert( 10407 ,  s , abs( min( t ).firstElement().canonicalType() - max( t ).firstElement().canonicalType() ) <= 10 );
            }
        }



    };

    class ExtractFieldsTest {
    public:
        void run(){
            BSONObj x = BSON( "a" << 10 << "b" << 11 );
            assert( BSON( "a" << 10 ).woCompare( x.extractFields( BSON( "a" << 1 ) ) ) == 0 );
            assert( BSON( "b" << 11 ).woCompare( x.extractFields( BSON( "b" << 1 ) ) ) == 0 );
            assert( x.woCompare( x.extractFields( BSON( "a" << 1 << "b" << 1 ) ) ) == 0 );

            assert( (string)"a" == x.extractFields( BSON( "a" << 1 << "c" << 1 ) ).firstElement().fieldName() );
        }
    };

    class ComparatorTest {
    public:
        BSONObj one( string s ){
            return BSON( "x" << s );
        }
        BSONObj two( string x , string y ){
            BSONObjBuilder b;
            b.append( "x" , x );
            if ( y.size() )
                b.append( "y" , y );
            else
                b.appendNull( "y" );
            return b.obj();
        }

        void test( BSONObj order , BSONObj l , BSONObj r , bool wanted ){
            BSONObjCmp c( order );
            bool got = c(l,r);
            if ( got == wanted )
                return;
            cout << " order: " << order << " l: " << l << "r: " << r << " wanted: " << wanted << " got: " << got << endl;
        }

        void lt( BSONObj order , BSONObj l , BSONObj r ){
            test( order , l , r , 1 );
        }

        void run(){
            BSONObj s = BSON( "x" << 1 );
            BSONObj c = BSON( "x" << 1 << "y" << 1 );
            test( s , one( "A" ) , one( "B" ) , 1 );
            test( s , one( "B" ) , one( "A" ) , 0 );

            test( c , two( "A" , "A" ) , two( "A" , "B" ) , 1 );
            test( c , two( "A" , "A" ) , two( "B" , "A" ) , 1 );
            test( c , two( "B" , "A" ) , two( "A" , "B" ) , 0 );

            lt( c , one("A") , two( "A" , "A" ) );
            lt( c , one("A") , one( "B" ) );
            lt( c , two("A","") , two( "B" , "A" ) );

            lt( c , two("B","A") , two( "C" , "A" ) );
            lt( c , two("B","A") , one( "C" ) );
            lt( c , two("B","A") , two( "C" , "" ) );

        }
    };

    namespace external_sort {
        class Basic1 {
        public:
            void run(){
                BSONObjExternalSorter sorter;
                sorter.add( BSON( "x" << 10 ) , 5  , 1);
                sorter.add( BSON( "x" << 2 ) , 3 , 1 );
                sorter.add( BSON( "x" << 5 ) , 6 , 1 );
                sorter.add( BSON( "x" << 5 ) , 7 , 1 );

                sorter.sort();
                
                auto_ptr<BSONObjExternalSorter::Iterator> i = sorter.iterator();
                int num=0;
                while ( i->more() ){
                    pair<BSONObj,DiskLoc> p = i->next();
                    if ( num == 0 )
                        assert( p.first["x"].number() == 2 );
                    else if ( num <= 2 ){
                        assert( p.first["x"].number() == 5 );
                    }
                    else if ( num == 3 )
                        assert( p.first["x"].number() == 10 );
                    else
                        ASSERT( 0 );
                    num++;
                }
                
                
                ASSERT_EQUALS( 0 , sorter.numFiles() );
            }
        };

        class Basic2 {
        public:
            void run(){
                BSONObjExternalSorter sorter( BSONObj() , 10 );
                sorter.add( BSON( "x" << 10 ) , 5  , 11 );
                sorter.add( BSON( "x" << 2 ) , 3 , 1 );
                sorter.add( BSON( "x" << 5 ) , 6 , 1 );
                sorter.add( BSON( "x" << 5 ) , 7 , 1 );

                sorter.sort();
                
                auto_ptr<BSONObjExternalSorter::Iterator> i = sorter.iterator();
                int num=0;
                while ( i->more() ){
                    pair<BSONObj,DiskLoc> p = i->next();
                    if ( num == 0 ){
                        assert( p.first["x"].number() == 2 );
                        ASSERT_EQUALS( p.second.toString() , "3:1" );
                    }
                    else if ( num <= 2 )
                        assert( p.first["x"].number() == 5 );
                    else if ( num == 3 ){
                        assert( p.first["x"].number() == 10 );
                        ASSERT_EQUALS( p.second.toString() , "5:b" );
                    }
                    else
                        ASSERT( 0 );
                    num++;
                }

            }
        };

        class Basic3 {
        public:
            void run(){
                BSONObjExternalSorter sorter( BSONObj() , 10 );
                sorter.sort();

                auto_ptr<BSONObjExternalSorter::Iterator> i = sorter.iterator();
                assert( ! i->more() );

            }
        };


        class ByDiskLock {
        public:
            void run(){
                BSONObjExternalSorter sorter;
                sorter.add( BSON( "x" << 10 ) , 5  , 4);
                sorter.add( BSON( "x" << 2 ) , 3 , 0 );
                sorter.add( BSON( "x" << 5 ) , 6 , 2 );
                sorter.add( BSON( "x" << 5 ) , 7 , 3 );
                sorter.add( BSON( "x" << 5 ) , 2 , 1 );
                
                sorter.sort();

                auto_ptr<BSONObjExternalSorter::Iterator> i = sorter.iterator();
                int num=0;
                while ( i->more() ){
                    pair<BSONObj,DiskLoc> p = i->next();
                    if ( num == 0 )
                        assert( p.first["x"].number() == 2 );
                    else if ( num <= 3 ){
                        assert( p.first["x"].number() == 5 );
                    }
                    else if ( num == 4 )
                        assert( p.first["x"].number() == 10 );
                    else
                        ASSERT( 0 );
                    ASSERT_EQUALS( num , p.second.getOfs() );
                    num++;
                }


            }
        };


        class Big1 {
        public:
            void run(){
                BSONObjExternalSorter sorter( BSONObj() , 2000 );
                for ( int i=0; i<10000; i++ ){
                    sorter.add( BSON( "x" << rand() % 10000 ) , 5  , i );
                }

                sorter.sort();

                auto_ptr<BSONObjExternalSorter::Iterator> i = sorter.iterator();
                int num=0;
                double prev = 0;
                while ( i->more() ){
                    pair<BSONObj,DiskLoc> p = i->next();
                    num++;
                    double cur = p.first["x"].number();
                    assert( cur >= prev );
                    prev = cur;
                }
                assert( num == 10000 );
            }
        };
        
        class Big2 {
        public:
            void run(){
                const int total = 100000;
                BSONObjExternalSorter sorter( BSONObj() , total * 2 );
                for ( int i=0; i<total; i++ ){
                    sorter.add( BSON( "a" << "b" ) , 5  , i );
                }

                sorter.sort();
                
                auto_ptr<BSONObjExternalSorter::Iterator> i = sorter.iterator();
                int num=0;
                double prev = 0;
                while ( i->more() ){
                    pair<BSONObj,DiskLoc> p = i->next();
                    num++;
                    double cur = p.first["x"].number();
                    assert( cur >= prev );
                    prev = cur;
                }
                assert( num == total );
                ASSERT( sorter.numFiles() > 2 );
            }
        };

        class D1 {
        public:
            void run(){
                
                BSONObjBuilder b;
                b.appendNull("");
                BSONObj x = b.obj();
                
                BSONObjExternalSorter sorter;
                sorter.add(x, DiskLoc(3,7));
                sorter.add(x, DiskLoc(4,7));
                sorter.add(x, DiskLoc(2,7));
                sorter.add(x, DiskLoc(1,7));
                sorter.add(x, DiskLoc(3,77));
                
                sorter.sort();
                
                auto_ptr<BSONObjExternalSorter::Iterator> i = sorter.iterator();
                while( i->more() ) {
                    BSONObjExternalSorter::Data d = i->next();
                    /*cout << d.second.toString() << endl;
                    cout << d.first.objsize() << endl;
                    cout<<"SORTER next:" << d.first.toString() << endl;*/
                }
            }
        };
    }
    
    class CompatBSON {
    public:
        
#define JSONBSONTEST(j,s,m) ASSERT_EQUALS( fromjson( j ).objsize() , s ); ASSERT_EQUALS( fromjson( j ).md5() , m );
#define RAWBSONTEST(j,s,m) ASSERT_EQUALS( j.objsize() , s ); ASSERT_EQUALS( j.md5() , m );

        void run(){

            JSONBSONTEST( "{ 'x' : true }" , 9 , "6fe24623e4efc5cf07f027f9c66b5456" );
            JSONBSONTEST( "{ 'x' : null }" , 8 , "12d43430ff6729af501faf0638e68888" );
            JSONBSONTEST( "{ 'x' : 5.2 }" , 16 , "aaeeac4a58e9c30eec6b0b0319d0dff2" );
            JSONBSONTEST( "{ 'x' : 'eliot' }" , 18 , "331a3b8b7cbbe0706c80acdb45d4ebbe" );
            JSONBSONTEST( "{ 'x' : 5.2 , 'y' : 'truth' , 'z' : 1.1 }" , 40 , "7c77b3a6e63e2f988ede92624409da58" );
            JSONBSONTEST( "{ 'a' : { 'b' : 1.1 } }" , 24 , "31887a4b9d55cd9f17752d6a8a45d51f" );
            JSONBSONTEST( "{ 'x' : 5.2 , 'y' : { 'a' : 'eliot' , b : true } , 'z' : null }" , 44 , "b3de8a0739ab329e7aea138d87235205" );
            JSONBSONTEST( "{ 'x' : 5.2 , 'y' : [ 'a' , 'eliot' , 'b' , true ] , 'z' : null }" , 62 , "cb7bad5697714ba0cbf51d113b6a0ee8" );
            
            RAWBSONTEST( BSON( "x" << 4 ) , 12 , "d1ed8dbf79b78fa215e2ded74548d89d" );
            
        }
    };
    
    class CompareDottedFieldNamesTest {
    public:
        void t( FieldCompareResult res , const string& l , const string& r ){
            ASSERT_EQUALS( res , compareDottedFieldNames( l , r ) );
            ASSERT_EQUALS( -1 * res , compareDottedFieldNames( r , l ) );
        }
        
        void run(){
            t( SAME , "x" , "x" );
            t( SAME , "x.a" , "x.a" );
            t( LEFT_BEFORE , "a" , "b" );
            t( RIGHT_BEFORE , "b" , "a" );

            t( LEFT_SUBFIELD , "a.x" , "a" );
        }
    };

    struct NestedDottedConversions{
        void t(const BSONObj& nest, const BSONObj& dot){
            ASSERT_EQUALS( nested2dotted(nest), dot);
            ASSERT_EQUALS( nest, dotted2nested(dot));
        }

        void run(){
            t( BSON("a" << BSON("b" << 1)), BSON("a.b" << 1) );
            t( BSON("a" << BSON("b" << 1 << "c" << 1)), BSON("a.b" << 1 << "a.c" << 1) );
            t( BSON("a" << BSON("b" << 1 << "c" << 1) << "d" << 1), BSON("a.b" << 1 << "a.c" << 1 << "d" << 1) );
            t( BSON("a" << BSON("b" << 1 << "c" << 1 << "e" << BSON("f" << 1)) << "d" << 1), BSON("a.b" << 1 << "a.c" << 1 << "a.e.f" << 1 << "d" << 1) );
        }
    };

    struct BSONArrayBuilderTest{
        void run(){
            int i = 0;
            BSONObjBuilder objb;
            BSONArrayBuilder arrb;

            objb << objb.numStr(i++) << 100;
            arrb                     << 100;

            objb << objb.numStr(i++) << 1.0;
            arrb                     << 1.0;

            objb << objb.numStr(i++) << "Hello";
            arrb                     << "Hello";

            objb << objb.numStr(i++) << string("World");
            arrb                     << string("World");

            objb << objb.numStr(i++) << BSON( "a" << 1 << "b" << "foo" );
            arrb                     << BSON( "a" << 1 << "b" << "foo" );

            objb << objb.numStr(i++) << BSON( "a" << 1)["a"];
            arrb                     << BSON( "a" << 1)["a"];

            OID oid;
            oid.init();
            objb << objb.numStr(i++) << oid;
            arrb                     << oid;

            BSONObj obj = objb.obj();
            BSONArray arr = arrb.arr();

            ASSERT_EQUALS(obj, arr);

            BSONObj o = BSON( "obj" << obj << "arr" << arr << "arr2" << BSONArray(obj) );
            ASSERT_EQUALS(o["obj"].type(), Object);
            ASSERT_EQUALS(o["arr"].type(), Array);
            ASSERT_EQUALS(o["arr2"].type(), Array);
        }
    };
    
    struct ArrayMacroTest{
        void run(){
            BSONArray arr = BSON_ARRAY( "hello" << 1 << BSON( "foo" << BSON_ARRAY( "bar" << "baz" << "qux" ) ) );
            BSONObj obj = BSON( "0" << "hello"
                             << "1" << 1
                             << "2" << BSON( "foo" << BSON_ARRAY( "bar" << "baz" << "qux" ) ) );

            ASSERT_EQUALS(arr, obj);
            ASSERT_EQUALS(arr["2"].type(), Object);
            ASSERT_EQUALS(arr["2"].embeddedObject()["foo"].type(), Array);
        }
    };

    class NumberParsing {
    public:
        void run(){
            BSONObjBuilder a;
            BSONObjBuilder b;

            a.append( "a" , (int)1 );
            ASSERT( b.appendAsNumber( "a" , "1" ) );
            
            a.append( "b" , 1.1 );
            ASSERT( b.appendAsNumber( "b" , "1.1" ) );

            a.append( "c" , (int)-1 );
            ASSERT( b.appendAsNumber( "c" , "-1" ) );
            
            a.append( "d" , -1.1 );
            ASSERT( b.appendAsNumber( "d" , "-1.1" ) );

            a.append( "e" , (long long)32131231231232313LL );
            ASSERT( b.appendAsNumber( "e" , "32131231231232313" ) );
            
            ASSERT( ! b.appendAsNumber( "f" , "zz" ) );
            ASSERT( ! b.appendAsNumber( "f" , "5zz" ) );
            ASSERT( ! b.appendAsNumber( "f" , "zz5" ) );

            ASSERT_EQUALS( a.obj() , b.obj() );
        }
    };
    
    class bson2settest {
    public:
        void run(){
            BSONObj o = BSON( "z" << 1 << "a" << 2 << "m" << 3 << "c" << 4 );
            BSONObjIteratorSorted i( o );
            stringstream ss;
            while ( i.more() )
                ss << i.next().fieldName();
            ASSERT_EQUALS( "acmz" , ss.str() );

            {
                Timer t;
                for ( int i=0; i<10000; i++ ){
                    BSONObjIteratorSorted j( o );
                    int l = 0;
                    while ( j.more() )
                        l += strlen( j.next().fieldName() );
                }
                unsigned long long tm = t.micros();
                cout << "time: " << tm << endl;
            }
        }

    };

    class checkForStorageTests {
    public:
        
        void good( string s ){
            BSONObj o = fromjson( s );
            if ( o.okForStorage() )
                return;
            throw UserException( 12528 , (string)"should be ok for storage:" + s );
        }

        void bad( string s ){
            BSONObj o = fromjson( s );
            if ( ! o.okForStorage() )
                return;
            throw UserException( 12529 , (string)"should NOT be ok for storage:" + s );
        }

        void run(){
            good( "{x:1}" );
            bad( "{'x.y':1}" );

            good( "{x:{a:2}}" );
            bad( "{x:{'$a':2}}" );
        }
    };

    class InvalidIDFind {
    public:
        void run(){
            BSONObj x = BSON( "_id" << 5 << "t" << 2 );
            {
                char * crap = (char*)malloc( x.objsize() );
                memcpy( crap , x.objdata() , x.objsize() );
                BSONObj y( crap , false );
                ASSERT_EQUALS( x , y );
                free( crap );
            }
            
            {
                char * crap = (char*)malloc( x.objsize() );
                memcpy( crap , x.objdata() , x.objsize() );
                int * foo = (int*)crap;
                foo[0] = 123123123;
                int state = 0;
                try {
                    BSONObj y( crap , false );
                    state = 1;
                }
                catch ( std::exception& e ){
                    state = 2;
                    ASSERT( strstr( e.what() , "_id: 5" ) > 0 );
                }
                free( crap );
                ASSERT_EQUALS( 2 , state );
            }
                
            
        }
    };

    class ElementSetTest {
    public:
        void run(){
            BSONObj x = BSON( "a" << 1 << "b" << 1 << "c" << 2 );
            BSONElement a = x["a"];
            BSONElement b = x["b"];
            BSONElement c = x["c"];
            cout << "c: " << c << endl;
            ASSERT( a.woCompare( b ) != 0 );
            ASSERT( a.woCompare( b , false ) == 0 );
            
            BSONElementSet s;
            s.insert( a );
            ASSERT_EQUALS( 1U , s.size() );
            s.insert( b );
            ASSERT_EQUALS( 1U , s.size() );
            ASSERT( ! s.count( c ) );

            ASSERT( s.find( a ) != s.end() );
            ASSERT( s.find( b ) != s.end() );
            ASSERT( s.find( c ) == s.end() );
                    
            
            s.insert( c );
            ASSERT_EQUALS( 2U , s.size() );


            ASSERT( s.find( a ) != s.end() );
            ASSERT( s.find( b ) != s.end() );
            ASSERT( s.find( c ) != s.end() );

            ASSERT( s.count( a ) );
            ASSERT( s.count( b ) );
            ASSERT( s.count( c ) );
        }
    };

    class EmbeddedNumbers {
    public:
        void run(){
            BSONObj x = BSON( "a" << BSON( "b" << 1 ) );
            BSONObj y = BSON( "a" << BSON( "b" << 1.0 ) );
            ASSERT_EQUALS( x , y );
            ASSERT_EQUALS( 0 , x.woCompare( y ) );
        }
    };

    class BuilderPartialItearte {
    public:
        void run(){
            {
                BSONObjBuilder b;
                b.append( "x" , 1 );
                b.append( "y" , 2 );
                
                BSONObjIterator i = b.iterator();
                ASSERT( i.more() );
                ASSERT_EQUALS( 1 , i.next().numberInt() );
                ASSERT( i.more() );
                ASSERT_EQUALS( 2 , i.next().numberInt() );
                ASSERT( ! i.more() );

                b.append( "z" , 3 );

                i = b.iterator();
                ASSERT( i.more() );
                ASSERT_EQUALS( 1 , i.next().numberInt() );
                ASSERT( i.more() );
                ASSERT_EQUALS( 2 , i.next().numberInt() );
                ASSERT( i.more() );
                ASSERT_EQUALS( 3 , i.next().numberInt() );
                ASSERT( ! i.more() );

                ASSERT_EQUALS( BSON( "x" << 1 << "y" << 2 << "z" << 3 ) , b.obj() );
            }
                
        }
    };

    class BSONFieldTests {
    public:
        void run(){
            {
                BSONField<int> x("x");
                BSONObj o = BSON( x << 5 );
                ASSERT_EQUALS( BSON( "x" << 5 ) , o );
            }

            {
                BSONField<int> x("x");
                BSONObj o = BSON( x.make(5) );
                ASSERT_EQUALS( BSON( "x" << 5 ) , o );
            }

            {
                BSONField<int> x("x");
                BSONObj o = BSON( x(5) );
                ASSERT_EQUALS( BSON( "x" << 5 ) , o );

                o = BSON( x.gt(5) );
                ASSERT_EQUALS( BSON( "x" << BSON( "$gt" << 5 ) ) , o );
            }

        }
    };

    class BSONForEachTest {
    public:
        void run(){
            BSONObj obj = BSON("a" << 1 << "a" << 2 << "a" << 3);
            
            int count = 0;
            BSONForEach(e, obj){
                ASSERT_EQUALS( e.fieldName() , string("a") );
                count += e.Int();
            }

            ASSERT_EQUALS( count , 1+2+3 );
        }
    };

    class StringDataTest {
    public:
        void run(){
            StringData a( string( "aaa" ) );
            ASSERT_EQUALS( 3u , a.size() );

            StringData b( string( "bbb" ).c_str() );
            ASSERT_EQUALS( 3u , b.size() );

            StringData c( "ccc", StringData::LiteralTag() );
            ASSERT_EQUALS( 3u , c.size() );

            // TODO update test when second parm takes StringData too
            BSONObjBuilder builder;
            builder.append( c, "value");
            ASSERT_EQUALS( builder.obj() , BSON( c.data() << "value" ) );

        }
    };

    class CompareOps {
    public:
        void run(){
            
            BSONObj a = BSON("a"<<1);
            BSONObj b = BSON("a"<<1);
            BSONObj c = BSON("a"<<2);
            BSONObj d = BSON("a"<<3);
            BSONObj e = BSON("a"<<4);
            BSONObj f = BSON("a"<<4);

            ASSERT( ! ( a < b ) );
            ASSERT( a <= b );
            ASSERT( a < c );
            
            ASSERT( f > d );
            ASSERT( f >= e );
            ASSERT( ! ( f > e ) );
        }
    };

    class All : public Suite {
    public:
        All() : Suite( "jsobj" ){
        }

        void setupTests(){
            add< BufBuilderBasic >();
            add< BSONElementBasic >();
            add< BSONObjTests::Create >();
            add< BSONObjTests::WoCompareBasic >();
            add< BSONObjTests::NumericCompareBasic >();
            add< BSONObjTests::WoCompareEmbeddedObject >();
            add< BSONObjTests::WoCompareEmbeddedArray >();
            add< BSONObjTests::WoCompareOrdered >();
            add< BSONObjTests::WoCompareDifferentLength >();
            add< BSONObjTests::WoSortOrder >();
            add< BSONObjTests::MultiKeySortOrder > ();
            add< BSONObjTests::TimestampTest >();
            add< BSONObjTests::Nan >();
            add< BSONObjTests::AsTempObj >();
            add< BSONObjTests::AppendIntOrLL >();
            add< BSONObjTests::AppendNumber >();
            add< BSONObjTests::ToStringArray >();
            add< BSONObjTests::NullString >();
            add< BSONObjTests::Validation::BadType >();
            add< BSONObjTests::Validation::EooBeforeEnd >();
            add< BSONObjTests::Validation::Undefined >();
            add< BSONObjTests::Validation::TotalSizeTooSmall >();
            add< BSONObjTests::Validation::EooMissing >();
            add< BSONObjTests::Validation::WrongStringSize >();
            add< BSONObjTests::Validation::ZeroStringSize >();
            add< BSONObjTests::Validation::NegativeStringSize >();
            add< BSONObjTests::Validation::WrongSubobjectSize >();
            add< BSONObjTests::Validation::WrongDbrefNsSize >();
            add< BSONObjTests::Validation::NoFieldNameEnd >();
            add< BSONObjTests::Validation::BadRegex >();
            add< BSONObjTests::Validation::BadRegexOptions >();
            add< BSONObjTests::Validation::CodeWScopeSmallSize >();
            add< BSONObjTests::Validation::CodeWScopeZeroStrSize >();
            add< BSONObjTests::Validation::CodeWScopeSmallStrSize >();
            add< BSONObjTests::Validation::CodeWScopeNoSizeForObj >();
            add< BSONObjTests::Validation::CodeWScopeSmallObjSize >();
            add< BSONObjTests::Validation::CodeWScopeBadObject >();
            add< BSONObjTests::Validation::NoSize >( Symbol );
            add< BSONObjTests::Validation::NoSize >( Code );
            add< BSONObjTests::Validation::NoSize >( String );
            add< BSONObjTests::Validation::NoSize >( CodeWScope );
            add< BSONObjTests::Validation::NoSize >( DBRef );
            add< BSONObjTests::Validation::NoSize >( Object );
            add< BSONObjTests::Validation::NoSize >( Array );
            add< BSONObjTests::Validation::NoSize >( BinData );
            add< BSONObjTests::Validation::Fuzz >( .5 );
            add< BSONObjTests::Validation::Fuzz >( .1 );
            add< BSONObjTests::Validation::Fuzz >( .05 );
            add< BSONObjTests::Validation::Fuzz >( .01 );
            add< BSONObjTests::Validation::Fuzz >( .001 );
            add< OIDTests::init1 >();
            add< OIDTests::initParse1 >();
            add< OIDTests::append >();
            add< OIDTests::increasing >();
            add< OIDTests::ToDate >();
            add< OIDTests::FromDate >();
            add< ValueStreamTests::LabelBasic >();
            add< ValueStreamTests::LabelShares >();
            add< ValueStreamTests::LabelDouble >();
            add< ValueStreamTests::LabelDoubleShares >();
            add< ValueStreamTests::LabelSize >();
            add< ValueStreamTests::LabelMulti >();
            add< ValueStreamTests::LabelishOr >();
            add< ValueStreamTests::Unallowed >();
            add< ValueStreamTests::ElementAppend >();
            add< SubObjectBuilder >();
            add< DateBuilder >();
            add< DateNowBuilder >();
            add< TimeTBuilder >();
            add< ValueStreamTests::Unallowed >();
            add< ValueStreamTests::ElementAppend >();
            add< SubObjectBuilder >();
            add< DateBuilder >();
            add< DateNowBuilder >();
            add< TimeTBuilder >();
            add< MinMaxElementTest >();
            add< ComparatorTest >();
            add< ExtractFieldsTest >();
            add< external_sort::Basic1 >();
            add< external_sort::Basic2 >();
            add< external_sort::Basic3 >();
            add< external_sort::ByDiskLock >();
            add< external_sort::Big1 >();
            add< external_sort::Big2 >();
            add< external_sort::D1 >();
            add< CompatBSON >();
            add< CompareDottedFieldNamesTest >();
            add< NestedDottedConversions >();
            add< BSONArrayBuilderTest >();
            add< ArrayMacroTest >();
            add< NumberParsing >();
            add< bson2settest >();
            add< checkForStorageTests >();
            add< InvalidIDFind >();
            add< ElementSetTest >();
            add< EmbeddedNumbers >();
            add< BuilderPartialItearte >();
            add< BSONFieldTests >();
            add< BSONForEachTest >();
            add< StringDataTest >();
            add< CompareOps >();
        }
    } myall;
    
} // namespace JsobjTests

