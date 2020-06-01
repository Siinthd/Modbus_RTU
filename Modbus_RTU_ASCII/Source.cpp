#include <exception>
#include <boost/asio.hpp>
#include <boost/asio/serial_port.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/bind.hpp>
#include <boost/utility.hpp>
#include <boost/algorithm/string.hpp>

namespace SerialPort
{
	class SerialException : public std::exception {
	public:
		SerialException(const std::string & msg = "SerialException") : std::exception(), m_what(msg)
		{
		}
		~SerialException() throw() {}
		const char * what() const throw() { return m_what.c_str(); }
	private:
		const std::string m_what;
	};

	class SerialTimeoutException : public SerialException {
	public:
		SerialTimeoutException(const std::string & msg = "SerialTimeoutException") : SerialException(msg)
		{
		}
	};

	namespace ba = boost::asio;
	class TimeoutSerialPortBoost {
	public:
		TimeoutSerialPortBoost() : m_io(), m_port(m_io), m_timer(m_io)
		{
		}
		void open(
			const std::string & devname,
			const size_t baud_rate,
			const size_t timeout_ms,
			const size_t char_size,
			const ba::serial_port_base::parity parity_opt,
			const ba::serial_port_base::stop_bits stop_bits_opt
			)
		{
			//
			m_port.open(devname);
			m_port.set_option(ba::serial_port_base::baud_rate(baud_rate));
			m_timeout = boost::posix_time::milliseconds(timeout_ms);
			m_port.set_option(ba::serial_port_base::character_size(char_size));
			// none, odd, even
			m_port.set_option(parity_opt);
			//none, software, hardware
			m_port.set_option(ba::serial_port_base::flow_control(ba::serial_port_base::flow_control::none));
			//one, onepointfive, two
			m_port.set_option(stop_bits_opt);
		}

		void write(const unsigned char *data, const size_t size)
		{
			ba::write(m_port, ba::buffer(data, size));
		}

		void read(unsigned char * data, const size_t size)
		{
			if (m_timeout != boost::posix_time::seconds(0)) //If timeout is set, start timer
			{
				m_timer.expires_from_now(m_timeout);
				m_timer.async_wait(boost::bind(&TimeoutSerialPortBoost::timeout_expired,
					this,
					ba::placeholders::error
					)
					);
			}
			ba::async_read(m_port,
				ba::buffer(data, size),
				boost::bind(&TimeoutSerialPortBoost::read_completed,
				this,
				ba::placeholders::error,
				ba::placeholders::bytes_transferred
				)
				);
			m_read_result = resultInProgress;
			m_bytes_transferred = 0;
			while (true)
			{
				m_io.run_one();// 
				switch (m_read_result)
				{
				case resultSuccess:
					m_timer.cancel();
					return;
				case resultTimeoutExpired:
					m_port.cancel();
					throw SerialTimeoutException("Error in TimeutSerialBoost::read, timeout expired");
				case resultError:
					m_timer.cancel();
					m_port.cancel();
					throw SerialException("Error in TimeutSerialBoost::read, read error");
				default: //if resultInProgress remain in the loop
					break;
				}
			}
		}
	private:
		void read_completed(const boost::system::error_code& error, const size_t transferred)
		{
			if (error)
			{
				if (error != ba::error::operation_aborted)
					m_read_result = resultError;
			}
			else
			{
				if (m_read_result != resultInProgress)
					return;
				m_read_result = resultSuccess;
				this->m_bytes_transferred = transferred;
			}
		}
		void timeout_expired(const boost::system::error_code & error)
		{
			if (m_read_result != resultInProgress)
				return;
			if (error != ba::error::operation_aborted)
				m_read_result = resultTimeoutExpired;
		}
	private:
		enum ReadResult
		{
			resultInProgress = 0,
			resultSuccess = 1,
			resultError = 2,
			resultTimeoutExpired = 3
		};
	private:
		ba::io_service m_io;
		ba::serial_port m_port;
		ba::deadline_timer m_timer;
		boost::posix_time::time_duration m_timeout;
		ReadResult m_read_result;
		size_t m_bytes_transferred;
	};
}


/*
TimeoutSerialPortBoost::read(), которая хорошо демонстрирует сущность отношений между объектами собственно последовательного порта и т.н. сервиса ввода-вывода. Суть в следующем. Если мы хотим что-то прочитать из последовательного порта, то мы делаем это не напрямую (вызывая функции чтения класса порта), а посредством сервиса ввода-вывода. Зачем? Потому что мы не можем напрямую указать последовательному порту, что "надо прочитать некоторые данные, но ждать не более n миллисекунд". Для этого у нас есть сервис. Он "знает" об обоих объектах (таймер и последовательный порт) и мы будет вызывать его функцию run_one() до тех пор, пока не произойдет какое-то событие - чтение данных или истечет таймаут. Вызов run_one(). Фактически, вызов функции run_one() блокирует выполнение программы до тех пор, пока не произойдет какое-нибудь событие. После каждого вызова проверяется значение переменной состояния. Переменная состояния m_read_result может быть изменена обработчикам чтения или обработчиком таймаута (одно из этих событий обязательно произойдет после первого вызова run_one) и мы всегда будем знать, как у нас дела.
Естественно, мы можем напрямую вызывать функции чтения класса serial_port, но это будет во 1-х, идеологически неправильно, а во 2-х, мы не сможем завершать ожидание байт по истечению таймаута.

*/