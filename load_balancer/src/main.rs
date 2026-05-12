use tokio_uring::net::{TcpListener, TcpStream, UnixStream};
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Arc;
use std::time::Duration;

const API1: &str = "/sockets/api1.sock";
const API2: &str = "/sockets/api2.sock";

struct State {
    counter: AtomicUsize,
}

fn main() {
    tokio_uring::start(async {
        println!("Listening at 0.0.0.0:9999...");

        let listener = TcpListener::bind("0.0.0.0:9999".parse().unwrap())
            .expect("Error");

        let state = Arc::new(State {
            counter: AtomicUsize::new(0),
        });

        loop {
            let (client, _) = listener.accept().await.expect("Error");
            
            let state_clone = state.clone();
            let i = state_clone.counter.fetch_add(1, Ordering::Relaxed);
            let target_path = if i & 1 == 0 { API1 } else { API2 };

            tokio_uring::spawn(async move {
                if let Err(e) = handle(client, target_path).await {
                    eprintln!("Error: {:?}", e);
                }
            });
        }
    });
}

async fn handle(client: TcpStream, backend_path: &str) -> std::io::Result<()> {
    let mut buf = vec![0u8; 4096];

    loop {
        let (read_res, returned_buf) = client.read(buf).await;
        buf = returned_buf;

        let n = match read_res {
            Ok(0) => return Ok(()),
            Ok(n) => n,
            Err(e) => return Err(e),
        };

        let backend = match UnixStream::connect(backend_path).await {
            Ok(b) => b,
            Err(e) => {
                eprintln!("Error {}: {}", backend_path, e);
                return Err(e);
            }
        };


        let (write_res, _) = backend.write_all(buf[..n].to_vec()).await;
        write_res?;

        let (read_back_res, resp_buf) = backend.read(vec![0u8; 4096]).await;
        let rn = match read_back_res {
            Ok(n) if n > 0 => n,
            Ok(_) => return Ok(()),
            Err(e) => return Err(e),
        };

        let (client_write_res, _) = client.write_all(resp_buf[..rn].to_vec()).await;
        client_write_res?;
        
    }
}
